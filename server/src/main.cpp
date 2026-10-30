#include "network/diagnosticserver.h"
#include <cerrno>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <pthread.h>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <sys/signalfd.h>
#include <unistd.h>

namespace
{
class UniqueFd
{
  public:
    explicit UniqueFd(int value) : m_value(value) {}
    ~UniqueFd()
    {
        if (m_value >= 0)
            ::close(m_value);
    }
    int get() const
    {
        return m_value;
    }
    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;

  private:
    int m_value;
};
class SignalWatcher
{
  public:
    SignalWatcher(muduo::net::EventLoop *loop, const sigset_t &signals, const std::function<void()> &callback)
        : m_fd(::signalfd(-1, &signals, SFD_NONBLOCK | SFD_CLOEXEC)), m_callback(callback)
    {
        if (m_fd.get() < 0)
            throw std::runtime_error("signalfd failed");
        m_channel.reset(new muduo::net::Channel(loop, m_fd.get()));
        m_channel->setReadCallback(
            [this](muduo::Timestamp)
            {
                signalfd_siginfo info;
                while (::read(m_fd.get(), &info, sizeof(info)) == sizeof(info))
                    m_callback();
            });
        m_channel->enableReading();
    }
    ~SignalWatcher()
    {
        m_channel->disableAll();
        m_channel->remove();
    }

  private:
    UniqueFd m_fd;
    std::function<void()> m_callback;
    std::unique_ptr<muduo::net::Channel> m_channel;
};
unsigned parseNumber(const std::string &text, unsigned minimum, unsigned maximum)
{
    std::size_t consumed = 0;
    const unsigned long value = std::stoul(text, &consumed);
    if (consumed != text.size() || text.empty() || text[0] == '-' || value < minimum || value > maximum)
        throw std::runtime_error("invalid numeric option");
    return static_cast<unsigned>(value);
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        littlechat::server::ServerConfig config;
        for (int i = 1; i < argc; ++i)
        {
            const std::string option(argv[i]);
            if (option == "--help")
            {
                std::cout
                    << "littlechat_server (diagnostic only, loopback)\n"
                    << "--port 9988 --io-threads 2 --max-connections 128 "
                    << "--idle-timeout-ms 30000 --partial-timeout-ms 2000 --handshake-timeout-ms 5000\n";
                return 0;
            }
            if (i + 1 >= argc)
                throw std::runtime_error("option requires a value");
            const std::string value(argv[++i]);
            if (option == "--port")
                config.port = static_cast<unsigned short>(parseNumber(value, 1024, 65535));
            else if (option == "--io-threads")
                config.ioThreads = int(parseNumber(value, 0, 16));
            else if (option == "--max-connections")
                config.maxConnections = parseNumber(value, 1, 10000);
            else if (option == "--idle-timeout-ms")
                config.idleTimeout = parseNumber(value, 100, 3600000) / 1000.0;
            else if (option == "--partial-timeout-ms")
                config.partialFrameTimeout = parseNumber(value, 100, 60000) / 1000.0;
            else if (option == "--handshake-timeout-ms")
                config.handshakeTimeout = parseNumber(value, 100, 60000) / 1000.0;
            else
                throw std::runtime_error("unknown option");
        }
        if (std::getenv("MUDUO_USE_POLL") != nullptr)
            throw std::runtime_error(
                "This server requires epoll; remove MUDUO_USE_POLL from its environment");
        // Block before starting muduo IO threads. Signals are consumed by the main EventLoop.
        sigset_t signals;
        ::sigemptyset(&signals);
        ::sigaddset(&signals, SIGINT);
        ::sigaddset(&signals, SIGTERM);
        if (::pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0)
            throw std::runtime_error("pthread_sigmask failed");
        muduo::Logger::setLogLevel(muduo::Logger::INFO);
        muduo::net::EventLoop loop;
        littlechat::server::DiagnosticServer server(&loop, config);
        SignalWatcher watcher(&loop, signals, [&server] { server.stop(); });
        server.start();
        loop.loop();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "littlechat_server: " << error.what() << "\n";
        return 1;
    }
}
