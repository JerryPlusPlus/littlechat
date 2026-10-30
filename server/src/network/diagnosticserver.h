#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TimerId.h>
#include <mutex>
#include <vector>

namespace littlechat
{
namespace server
{
struct ServerConfig
{
    unsigned short port = 9988;
    int ioThreads = 2;
    std::size_t maxConnections = 128;
    double idleTimeout = 30.0;
    double partialFrameTimeout = 2.0;
    double handshakeTimeout = 5.0;
};

class DiagnosticServer
{
  public:
    DiagnosticServer(muduo::net::EventLoop *loop, const ServerConfig &config);
    ~DiagnosticServer();
    void start();
    void stop();

  private:
    typedef muduo::net::TcpConnectionPtr Connection;
    typedef std::chrono::steady_clock Clock;
    struct State
    {
        std::uint64_t id = 0;
        bool registered = false;
        bool handshaken = false;
        bool drainScheduled = false;
        bool hasPartial = false;
        Clock::time_point created = Clock::now();
        Clock::time_point lastActivity = created;
        Clock::time_point partialSince = created;
    };
    void onConnection(const Connection &connection);
    void onMessage(const Connection &connection, muduo::net::Buffer *buffer, muduo::Timestamp time);
    void drain(const Connection &connection);
    void checkTimeouts();
    std::vector<std::weak_ptr<muduo::net::TcpConnection>> snapshot();
    void maybeQuit();
    muduo::net::EventLoop *m_loop;
    ServerConfig m_config;
    std::atomic<bool> m_stopping;
    std::atomic<std::uint64_t> m_nextId;
    std::mutex m_mutex;
    std::map<std::uint64_t, std::weak_ptr<muduo::net::TcpConnection>> m_connections;
    muduo::net::TimerId m_monitor;
    muduo::net::TimerId m_stopDeadline;
    bool m_started;
    bool m_acceptStopped;
    std::size_t m_pendingStopBarriers;
    // Declared last: IO threads are joined before connection state/mutex destruction.
    muduo::net::TcpServer m_server;
};
} // namespace server
} // namespace littlechat
