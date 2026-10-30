#include "network/diagnosticserver.h"
#include "littlechat/protocol/envelope.h"
#include "littlechat/protocol/frame.h"
#include <boost/any.hpp>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/InetAddress.h>

namespace littlechat
{
namespace server
{
namespace
{
const std::size_t kOutputLimit = 2U * protocol::kMaxFrameLength;
const std::size_t kInputLimit = 2U * (protocol::kMaxFrameLength + protocol::kFramePrefixSize);
const unsigned kFramesPerTurn = 64;
void send(const muduo::net::TcpConnectionPtr &connection, const wire::Envelope &response)
{
    std::string frame;
    if (!protocol::serializeEnvelope(response, frame))
    {
        connection->forceClose();
        return;
    }
    connection->send(frame);
    if (connection->outputBuffer()->readableBytes() > kOutputLimit)
        connection->forceClose();
}
wire::Envelope responseFor(const wire::Envelope &request, wire::MessageType type)
{
    wire::Envelope response;
    response.set_type(type);
    response.set_request_id(request.request_id());
    response.set_message_id(request.message_id());
    response.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count());
    return response;
}
void error(const muduo::net::TcpConnectionPtr &connection, const wire::Envelope &request,
           wire::ErrorCode code, const char *description)
{
    wire::Envelope response = responseFor(request, wire::ERROR_RESPONSE);
    response.set_error_code(code);
    response.mutable_error()->set_description(description);
    send(connection, response);
}
} // namespace
DiagnosticServer::DiagnosticServer(muduo::net::EventLoop *loop, const ServerConfig &config)
    : m_loop(loop), m_config(config), m_stopping(false), m_nextId(1), m_started(false),
      m_acceptStopped(false), m_pendingStopBarriers(0),
      m_server(loop, muduo::net::InetAddress("127.0.0.1", config.port), "littlechat-diagnostic")
{
    m_server.setThreadNum(config.ioThreads);
    m_server.setConnectionCallback([this](const Connection &connection) { onConnection(connection); });
    m_server.setMessageCallback([this](const Connection &connection, muduo::net::Buffer *buffer,
                                       muduo::Timestamp time) { onMessage(connection, buffer, time); });
}
DiagnosticServer::~DiagnosticServer()
{
    m_stopping.store(true);
    if (m_started)
        m_loop->cancel(m_monitor);
    m_loop->cancel(m_stopDeadline);
}
void DiagnosticServer::start()
{
    m_loop->assertInLoopThread();
    m_started = true;
    m_monitor = m_loop->runEvery(0.1, [this] { checkTimeouts(); });
    m_server.start();
    LOG_INFO << "[network] diagnostic-only endpoint=127.0.0.1:" << m_config.port
             << " io_threads=" << m_config.ioThreads << " no authentication or persistence";
}
std::vector<std::weak_ptr<muduo::net::TcpConnection>> DiagnosticServer::snapshot()
{
    std::vector<std::weak_ptr<muduo::net::TcpConnection>> result;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &entry : m_connections)
        result.push_back(entry.second);
    return result;
}
void DiagnosticServer::onConnection(const Connection &connection)
{
    connection->getLoop()->assertInLoopThread();
    if (connection->connected())
    {
        State state;
        state.id = m_nextId.fetch_add(1);
        bool accepted = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            accepted = !m_stopping.load() && m_connections.size() < m_config.maxConnections;
            // Track rejected connections too, until TcpServer has finished removing them.
            m_connections.emplace(state.id, connection);
            state.registered = true;
        }
        connection->setContext(state);
        if (!accepted)
        {
            connection->forceClose();
            return;
        }
        connection->setTcpNoDelay(true);
        connection->setHighWaterMarkCallback(
            [](const Connection &slow, std::size_t)
            {
                LOG_WARN << "[network] slow-reader connection=" << slow->name();
                slow->forceClose();
            },
            kOutputLimit);
    }
    else
    {
        State *state = boost::any_cast<State>(connection->getMutableContext());
        if (state && state->registered)
        {
            state->registered = false;
            const std::uint64_t id = state->id;
            // handleClose invokes TcpServer's close callback AFTER this connection callback.
            // Defer twice so registry cleanup follows TcpServer::removeConnectionInLoop.
            connection->getLoop()->queueInLoop(
                [this, id]
                {
                    m_loop->queueInLoop(
                        [this, id]
                        {
                            {
                                std::lock_guard<std::mutex> lock(m_mutex);
                                m_connections.erase(id);
                            }
                            if (m_stopping.load())
                                maybeQuit();
                        });
                });
        }
    }
}
void DiagnosticServer::onMessage(const Connection &connection, muduo::net::Buffer *buffer, muduo::Timestamp)
{
    connection->getLoop()->assertInLoopThread();
    State *state = boost::any_cast<State>(connection->getMutableContext());
    if (!state || !state->registered)
        return;
    state->lastActivity = Clock::now();
    if (m_stopping.load())
    {
        // Continue consuming input/EOF while flushing output. Read interest remains enabled until close.
        buffer->retrieveAll();
        if (connection->connected())
            connection->shutdown();
        return;
    }
    if (buffer->readableBytes() > kInputLimit)
    {
        connection->forceClose();
        return;
    }
    if (!state->drainScheduled)
        drain(connection);
}
void DiagnosticServer::drain(const Connection &connection)
{
    connection->getLoop()->assertInLoopThread();
    State *state = boost::any_cast<State>(connection->getMutableContext());
    if (!state || !connection->connected())
        return;
    state->drainScheduled = false;
    if (m_stopping.load())
    {
        connection->inputBuffer()->retrieveAll();
        connection->shutdown();
        return;
    }
    muduo::net::Buffer *buffer = connection->inputBuffer();
    unsigned handled = 0;
    while (connection->connected() && handled < kFramesPerTurn)
    {
        protocol::Frame frame;
        const protocol::DecodeResult result =
            protocol::decodeFrame(buffer->peek(), buffer->readableBytes(), frame);
        if (result.status == protocol::DecodeStatus::NeedMore)
        {
            if (buffer->readableBytes() > 0 && !state->hasPartial)
            {
                state->hasPartial = true;
                state->partialSince = Clock::now();
            }
            return;
        }
        if (result.status == protocol::DecodeStatus::Invalid)
        {
            LOG_WARN << "[protocol] invalid-frame connection=" << state->id << " error=" << int(result.error);
            connection->forceClose();
            return;
        }
        buffer->retrieve(result.consumed);
        state->hasPartial = false;
        ++handled;
        wire::Envelope request;
        if (!protocol::parseEnvelope(frame.payload, request))
        {
            LOG_WARN << "[protocol] malformed-envelope connection=" << state->id;
            connection->forceClose();
            return;
        }
        const protocol::RequestValidation validation = protocol::validateDiagnosticRequest(request);
        if (validation == protocol::RequestValidation::Malformed)
        {
            LOG_WARN << "[protocol] invalid-request connection=" << state->id
                     << " request=" << request.request_id();
            connection->forceClose();
            return;
        }
        if (validation == protocol::RequestValidation::Unauthenticated)
        {
            error(connection, request, wire::UNAUTHENTICATED,
                  "No authenticated identity exists on this diagnostic endpoint");
            continue;
        }
        if (request.type() == wire::HELLO_REQUEST)
        {
            if (state->handshaken)
            {
                error(connection, request, wire::BAD_STATE, "Handshake already completed");
            }
            else
            {
                state->handshaken = true;
                wire::Envelope response = responseFor(request, wire::HELLO_RESPONSE);
                response.mutable_hello_reply()->set_server_name("littlechat");
                response.mutable_hello_reply()->set_protocol_version(protocol::kProtocolVersion);
                response.mutable_hello_reply()->set_diagnostic_only(true);
                send(connection, response);
            }
        }
        else if (!state->handshaken)
        {
            error(connection, request, wire::HANDSHAKE_REQUIRED, "HELLO is required first");
        }
        else if (validation == protocol::RequestValidation::Unsupported)
        {
            error(connection, request, wire::NOT_IMPLEMENTED,
                  "Authentication and message persistence are not implemented");
        }
        else if (request.type() == wire::PING)
        {
            wire::Envelope response = responseFor(request, wire::PONG);
            response.mutable_pong()->set_nonce(request.ping().nonce());
            send(connection, response);
        }
        else if (request.type() == wire::ECHO_REQUEST)
        {
            wire::Envelope response = responseFor(request, wire::ECHO_RESPONSE);
            response.mutable_echo_response()->set_text(request.echo_request().text());
            send(connection, response);
        }
    }
    // A complete backlog must continue even when no additional socket read event arrives.
    if (connection->connected() && buffer->readableBytes() != 0)
    {
        state->drainScheduled = true;
        const std::weak_ptr<muduo::net::TcpConnection> weak(connection);
        connection->getLoop()->queueInLoop(
            [this, weak]
            {
                const Connection live = weak.lock();
                if (live)
                    drain(live);
            });
    }
}
void DiagnosticServer::checkTimeouts()
{
    if (m_stopping.load())
        return;
    for (const auto &weak : snapshot())
    {
        const Connection connection = weak.lock();
        if (!connection)
            continue;
        connection->getLoop()->queueInLoop(
            [this, weak]
            {
                const Connection live = weak.lock();
                if (!live || !live->connected())
                    return;
                State *state = boost::any_cast<State>(live->getMutableContext());
                if (!state)
                    return;
                const Clock::time_point now = Clock::now();
                const double idle = std::chrono::duration<double>(now - state->lastActivity).count();
                const double age = std::chrono::duration<double>(now - state->created).count();
                const double partialAge = std::chrono::duration<double>(now - state->partialSince).count();
                if (idle > m_config.idleTimeout || (!state->handshaken && age > m_config.handshakeTimeout) ||
                    (state->hasPartial && partialAge > m_config.partialFrameTimeout))
                    live->forceClose();
            });
    }
}
void DiagnosticServer::stop()
{
    m_loop->assertInLoopThread();
    if (m_stopping.exchange(true))
        return;
    LOG_INFO << "[network] stopping diagnostic server";
    // Destroy the listener after the active Channel callbacks for this poll have completed.
    m_loop->queueInLoop(
        [this]
        {
            m_server.stopAccepting();
            m_acceptStopped = true;
            const std::vector<muduo::net::EventLoop *> loops = m_server.threadPool()->getAllLoops();
            m_pendingStopBarriers = loops.size();
            // Each barrier follows all connectEstablished tasks already dispatched by the acceptor.
            for (muduo::net::EventLoop *ioLoop : loops)
            {
                ioLoop->queueInLoop(
                    [this]
                    {
                        m_loop->queueInLoop(
                            [this]
                            {
                                --m_pendingStopBarriers;
                                maybeQuit();
                            });
                    });
            }
            for (const auto &weak : snapshot())
            {
                const Connection connection = weak.lock();
                if (connection)
                {
                    connection->getLoop()->queueInLoop(
                        [weak]
                        {
                            const Connection live = weak.lock();
                            if (live && live->connected())
                            {
                                live->shutdown();
                                live->forceCloseWithDelay(1.0);
                            }
                        });
                }
            }
            m_stopDeadline = m_loop->runAfter(2.0,
                                              [this]
                                              {
                                                  for (const auto &weak : snapshot())
                                                  {
                                                      const Connection connection = weak.lock();
                                                      if (connection)
                                                      {
                                                          connection->getLoop()->queueInLoop(
                                                              [weak]
                                                              {
                                                                  const Connection live = weak.lock();
                                                                  if (live && !live->disconnected())
                                                                      live->forceClose();
                                                              });
                                                      }
                                                  }
                                                  maybeQuit();
                                              });
            maybeQuit();
        });
}
void DiagnosticServer::maybeQuit()
{
    m_loop->assertInLoopThread();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping.load() && m_acceptStopped && m_pendingStopBarriers == 0 && m_connections.empty())
        m_loop->quit();
}
} // namespace server
} // namespace littlechat
