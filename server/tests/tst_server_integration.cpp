#include "littlechat/protocol/envelope.h"
#include "littlechat/protocol/frame.h"
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <google/protobuf/unknown_field_set.h>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace littlechat;
namespace
{
std::string serverExecutable;
class Socket
{
  public:
    explicit Socket(int fd) : m_fd(fd) {}
    ~Socket()
    {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    int get() const
    {
        return m_fd;
    }
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

  private:
    int m_fd;
};
unsigned short unusedPort()
{
    Socket socket(::socket(AF_INET, SOCK_STREAM, 0));
    if (socket.get() < 0)
        throw std::runtime_error("socket failed");
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(socket.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
        throw std::runtime_error("temporary bind failed");
    socklen_t size = sizeof(address);
    if (::getsockname(socket.get(), reinterpret_cast<sockaddr *>(&address), &size) != 0)
        throw std::runtime_error("getsockname failed");
    return ntohs(address.sin_port);
}
class Client
{
  public:
    explicit Client(unsigned short port) : m_socket(::socket(AF_INET, SOCK_STREAM, 0))
    {
        if (m_socket.get() < 0)
            throw std::runtime_error("socket failed");
        timeval timeout = {3, 0};
        ::setsockopt(m_socket.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ::setsockopt(m_socket.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        if (::connect(m_socket.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
            throw std::runtime_error("connect failed");
    }
    void write(const std::string &data)
    {
        std::size_t offset = 0;
        while (offset < data.size())
        {
            const ssize_t count =
                ::send(m_socket.get(), data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                throw std::runtime_error("send failed");
            offset += std::size_t(count);
        }
    }
    void send(const wire::Envelope &message)
    {
        std::string encoded;
        if (!protocol::serializeEnvelope(message, encoded))
            throw std::runtime_error("encode failed");
        write(encoded);
    }
    wire::Envelope receive()
    {
        std::string input = read(4);
        const unsigned char *header = reinterpret_cast<const unsigned char *>(input.data());
        const std::uint32_t length = (std::uint32_t(header[0]) << 24) | (std::uint32_t(header[1]) << 16) |
                                     (std::uint32_t(header[2]) << 8) | std::uint32_t(header[3]);
        if (length <= 4 || length > protocol::kMaxFrameLength)
            throw std::runtime_error("invalid reply length");
        input += read(length);
        protocol::Frame frame;
        if (protocol::decodeFrame(input.data(), input.size(), frame).status !=
            protocol::DecodeStatus::Complete)
            throw std::runtime_error("invalid reply frame");
        wire::Envelope response;
        if (!response.ParseFromString(frame.payload))
            throw std::runtime_error("invalid reply protobuf");
        return response;
    }
    bool closed()
    {
        pollfd event = {m_socket.get(), POLLIN, 0};
        int ready;
        do
        {
            ready = ::poll(&event, 1, 3500);
        } while (ready < 0 && errno == EINTR);
        if (ready <= 0)
            return false;
        char byte;
        const ssize_t count = ::recv(m_socket.get(), &byte, 1, 0);
        return count == 0 || (count < 0 && errno == ECONNRESET);
    }

  private:
    std::string read(std::size_t size)
    {
        std::string result(size, '\0');
        std::size_t offset = 0;
        while (offset < size)
        {
            const ssize_t count = ::recv(m_socket.get(), &result[offset], size - offset, 0);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                throw std::runtime_error("receive timed out or peer closed");
            offset += std::size_t(count);
        }
        return result;
    }
    Socket m_socket;
};
wire::Envelope hello()
{
    wire::Envelope request;
    request.set_type(wire::HELLO_REQUEST);
    request.set_request_id(1);
    request.mutable_hello()->set_client_name("integration-test");
    return request;
}
void handshake(Client &client)
{
    client.send(hello());
    const wire::Envelope response = client.receive();
    if (response.type() != wire::HELLO_RESPONSE || !response.hello_reply().diagnostic_only())
        throw std::runtime_error("handshake failed");
}
wire::Envelope ping(std::uint64_t id)
{
    wire::Envelope request;
    request.set_type(wire::PING);
    request.set_request_id(id);
    request.mutable_ping()->set_nonce(id * 13);
    return request;
}
class ServerProcess
{
  public:
    explicit ServerProcess(unsigned maxConnections) : m_port(unusedPort()), m_pid(-1), m_status(0)
    {
        char filename[] = "/tmp/littlechat-integration-XXXXXX";
        Socket log(::mkstemp(filename));
        if (log.get() < 0)
            throw std::runtime_error("log file failed");
        m_log = filename;
        const std::string port = std::to_string(m_port);
        const std::string maximum = std::to_string(maxConnections);
        const char *arguments[] = {serverExecutable.c_str(),
                                   "--port",
                                   port.c_str(),
                                   "--io-threads",
                                   "2",
                                   "--max-connections",
                                   maximum.c_str(),
                                   "--idle-timeout-ms",
                                   "1500",
                                   "--partial-timeout-ms",
                                   "250",
                                   "--handshake-timeout-ms",
                                   "1000",
                                   nullptr};
        m_pid = ::fork();
        if (m_pid < 0)
            throw std::runtime_error("fork failed");
        if (m_pid == 0)
        {
            ::dup2(log.get(), STDOUT_FILENO);
            ::dup2(log.get(), STDERR_FILENO);
            ::execv(arguments[0], const_cast<char *const *>(arguments));
            ::_exit(127);
        }
        for (int retry = 0; retry < 150; ++retry)
        {
            if (::waitpid(m_pid, &m_status, WNOHANG) == m_pid)
            {
                m_pid = -1;
                throw std::runtime_error("server exited during startup: " + logs());
            }
            try
            {
                Client probe(m_port);
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                return;
            }
            catch (const std::exception &)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        stop();
        throw std::runtime_error("server startup timed out: " + logs());
    }
    ~ServerProcess()
    {
        stop();
        ::unlink(m_log.c_str());
    }
    unsigned short port() const
    {
        return m_port;
    }
    std::string logs() const
    {
        std::ifstream file(m_log.c_str());
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    int stop()
    {
        if (m_pid > 0)
        {
            ::kill(m_pid, SIGTERM);
            bool reaped = false;
            for (int retry = 0; retry < 200; ++retry)
            {
                if (::waitpid(m_pid, &m_status, WNOHANG) == m_pid)
                {
                    reaped = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!reaped)
            {
                ::kill(m_pid, SIGKILL);
                ::waitpid(m_pid, &m_status, 0);
            }
            m_pid = -1;
        }
        return WIFEXITED(m_status) ? WEXITSTATUS(m_status) : 128 + WTERMSIG(m_status);
    }

  private:
    unsigned short m_port;
    pid_t m_pid;
    int m_status;
    std::string m_log;
};
class NetworkTest : public testing::Test
{
  protected:
    virtual unsigned maxConnections() const
    {
        return 128;
    }
    void SetUp() override
    {
        server.reset(new ServerProcess(maxConnections()));
    }
    void TearDown() override
    {
        if (server)
        {
            EXPECT_EQ(server->stop(), 0) << server->logs();
            if (HasFailure())
                std::cerr << server->logs();
        }
    }
    std::unique_ptr<ServerProcess> server;
};
TEST_F(NetworkTest, FragmentedHelloAndMergedBacklogContinueWithoutNewReads)
{
    Client client(server->port());
    std::string encoded;
    ASSERT_TRUE(protocol::serializeEnvelope(hello(), encoded));
    client.write(encoded.substr(0, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    client.write(encoded.substr(1, 3));
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    client.write(encoded.substr(4));
    ASSERT_EQ(client.receive().type(), wire::HELLO_RESPONSE);
    std::string merged;
    for (unsigned i = 2; i < 142; ++i)
    {
        ASSERT_TRUE(protocol::serializeEnvelope(ping(i), encoded));
        merged += encoded;
    }
    client.write(merged);
    for (unsigned i = 2; i < 142; ++i)
    {
        const wire::Envelope response = client.receive();
        EXPECT_EQ(response.type(), wire::PONG);
        EXPECT_EQ(response.request_id(), i);
        EXPECT_EQ(response.pong().nonce(), i * 13U);
    }
}
TEST_F(NetworkTest, MaximumEnvelopeAndFollowingFrameAreBothProcessed)
{
    Client client(server->port());
    wire::Envelope request = hello();
    google::protobuf::UnknownFieldSet *unknown = request.GetReflection()->MutableUnknownFields(&request);
    unknown->AddLengthDelimited(100, std::string(protocol::kMaxFrameLength - 128, 'x'));
    std::string *padding = unknown->mutable_field(0)->mutable_length_delimited();
    const std::size_t target = protocol::kMaxFrameLength - protocol::kFrameMetadataSize;
    ASSERT_LT(request.ByteSizeLong(), target);
    padding->append(target - request.ByteSizeLong(), 'x');
    ASSERT_EQ(request.ByteSizeLong(), target);
    std::string first, second;
    ASSERT_TRUE(protocol::serializeEnvelope(request, first));
    ASSERT_TRUE(protocol::serializeEnvelope(ping(2), second));
    client.write(first + second);
    EXPECT_EQ(client.receive().type(), wire::HELLO_RESPONSE);
    EXPECT_EQ(client.receive().type(), wire::PONG);
}
TEST_F(NetworkTest, ConcurrentConnectionsReceiveOnlyTheirOwnResponses)
{
    std::vector<std::thread> clients;
    std::atomic<unsigned> failures(0);
    for (unsigned i = 0; i < 16; ++i)
    {
        clients.emplace_back(
            [this, i, &failures]
            {
                try
                {
                    Client client(server->port());
                    handshake(client);
                    for (unsigned j = 0; j < 10; ++j)
                    {
                        wire::Envelope request;
                        request.set_type(wire::ECHO_REQUEST);
                        request.set_request_id(j + 2);
                        request.set_message_id(std::string(16, char('a' + i)));
                        request.mutable_echo_request()->set_text("client-" + std::to_string(i) + "-" +
                                                                 std::to_string(j));
                        client.send(request);
                        const wire::Envelope response = client.receive();
                        if (response.type() != wire::ECHO_RESPONSE ||
                            response.request_id() != request.request_id() ||
                            response.message_id() != request.message_id() ||
                            response.echo_response().text() != request.echo_request().text())
                            ++failures;
                    }
                }
                catch (const std::exception &)
                {
                    ++failures;
                }
            });
    }
    for (std::thread &client : clients)
        client.join();
    EXPECT_EQ(failures.load(), 0U);
}
TEST_F(NetworkTest, RejectsInvalidLengthsVersionsFlagsAndPayload)
{
    std::string valid;
    ASSERT_TRUE(protocol::serializeEnvelope(hello(), valid));
    std::string badVersion = valid, badFlags = valid, malformed;
    badVersion[5] = 2;
    badFlags[7] = 1;
    ASSERT_TRUE(protocol::encodeFrame(std::string("\xff\xff\xff", 3), malformed));
    const std::vector<std::string> invalid = {std::string(4, '\0'), std::string("\x00\x10\x00\x01", 4),
                                              badVersion, badFlags, malformed};
    for (const std::string &frame : invalid)
    {
        Client client(server->port());
        client.write(frame);
        EXPECT_TRUE(client.closed());
    }
}
TEST_F(NetworkTest, NeverClaimsAuthenticationOrPersistence)
{
    Client client(server->port());
    client.send(ping(2));
    EXPECT_EQ(client.receive().error_code(), wire::HANDSHAKE_REQUIRED);
    handshake(client);
    wire::Envelope business;
    business.set_type(wire::SEND_TEXT);
    business.set_request_id(3);
    client.send(business);
    wire::Envelope response = client.receive();
    EXPECT_EQ(response.type(), wire::ERROR_RESPONSE);
    EXPECT_EQ(response.error_code(), wire::NOT_IMPLEMENTED);
    wire::Envelope spoofed = ping(4);
    spoofed.set_sender_id(99);
    client.send(spoofed);
    response = client.receive();
    EXPECT_EQ(response.type(), wire::ERROR_RESPONSE);
    EXPECT_EQ(response.error_code(), wire::UNAUTHENTICATED);
}
TEST_F(NetworkTest, ExpiresIncompleteAndIdleConnections)
{
    {
        Client partial(server->port());
        partial.write(std::string("\x00", 1));
        EXPECT_TRUE(partial.closed());
    }
    {
        Client idle(server->port());
        handshake(idle);
        EXPECT_TRUE(idle.closed());
    }
}
TEST_F(NetworkTest, HandlesPeerDisconnectWithIncompleteFrame)
{
    {
        Client client(server->port());
        client.write(std::string("\x00\x00", 2));
    }
    Client next(server->port());
    handshake(next);
    next.send(ping(9));
    EXPECT_EQ(next.receive().type(), wire::PONG);
}
TEST_F(NetworkTest, SigtermClosesConnectionsAndExitsSuccessfully)
{
    Client client(server->port());
    handshake(client);
    EXPECT_EQ(server->stop(), 0) << server->logs();
    EXPECT_TRUE(client.closed());
}
class LimitedNetworkTest : public NetworkTest
{
  protected:
    unsigned maxConnections() const override
    {
        return 2;
    }
};
TEST_F(LimitedNetworkTest, EnforcesConnectionLimit)
{
    Client first(server->port()), second(server->port());
    handshake(first);
    handshake(second);
    Client excess(server->port());
    EXPECT_TRUE(excess.closed());
    first.send(ping(5));
    EXPECT_EQ(first.receive().type(), wire::PONG);
}
TEST_F(NetworkTest, SigtermDuringConnectionChurn)
{
    std::atomic<bool> stop(false);
    std::atomic<unsigned> exchanges(0);
    std::vector<std::thread> clients;
    for (unsigned i = 0; i < 4; ++i)
    {
        clients.emplace_back(
            [this, &stop, &exchanges]
            {
                while (!stop.load())
                {
                    try
                    {
                        Client client(server->port());
                        handshake(client);
                        client.send(ping(2));
                        if (client.receive().type() == wire::PONG)
                            ++exchanges;
                    }
                    catch (const std::exception &)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    }
                }
            });
    }
    for (unsigned i = 0; i < 100 && exchanges.load() < 16; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const int status = server->stop();
    stop.store(true);
    for (std::thread &client : clients)
        client.join();
    EXPECT_GT(exchanges.load(), 0U);
    EXPECT_EQ(status, 0) << server->logs();
}
} // namespace
int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    serverExecutable = argv[1];
    for (int i = 1; i + 1 < argc; ++i)
        argv[i] = argv[i + 1];
    --argc;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
