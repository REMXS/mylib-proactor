// io_uring pingpong client
// 对齐 muduo examples/pingpong/client.cc 的用法:
//   pingpong_client <host_ip> <port> <threads> <blocksize> <sessions> <time>
// 行为: 建立 sessions 个连接, 每连接发一条 blocksize 消息,
//       收到服务端回显立即原样回发 (乒乓), 统计总字节数/消息数/吞吐 MiB/s
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

struct Stats
{
    std::atomic<int64_t> bytes{0};
    std::atomic<int64_t> messages{0};
};

static bool send_all(int fd, const char* data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

static int connect_to(const char* ip, uint16_t port)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
    {
        ::close(fd);
        return -1;
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ::close(fd);
        return -1;
    }
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return fd;
}

// 单个 worker: 管理 nconn 个连接, poll 轮询乒乓
static void worker(const char* ip, uint16_t port, size_t blocksize, int nconn,
                   const std::atomic<bool>& stop, Stats& stats)
{
    std::vector<int> fds;
    std::vector<pollfd> pfds;
    fds.reserve(nconn);
    pfds.reserve(nconn);

    std::vector<char> msg(blocksize, 'x');
    std::vector<char> buf(65536);

    for (int i = 0; i < nconn; ++i)
    {
        int fd = connect_to(ip, port);
        if (fd < 0)
            continue;
        fds.push_back(fd);
        pfds.push_back({fd, POLLIN, 0});
        // 连接建立后先发一条消息 (与 muduo onConnection 中 send 一致)
        send_all(fd, msg.data(), msg.size());
        stats.messages.fetch_add(1);
    }
    if (fds.empty())
        return;

    while (!stop.load())
    {
        int ret = ::poll(pfds.data(), pfds.size(), 100);
        if (ret <= 0)
            continue;
        for (size_t i = 0; i < pfds.size(); ++i)
        {
            if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP))
            {
                ssize_t n = ::recv(pfds[i].fd, buf.data(), buf.size(), 0);
                if (n <= 0)
                {
                    // 连接关闭: 关闭并从集合移除
                    ::close(pfds[i].fd);
                    pfds[i] = pfds.back();
                    pfds.pop_back();
                    fds[i] = fds.back();
                    fds.pop_back();
                    --i;
                    if (pfds.empty())
                        return;
                    continue;
                }
                stats.bytes.fetch_add(n);
                stats.messages.fetch_add(1);
                // 收到回显立即原样回发 (乒乓)
                send_all(pfds[i].fd, buf.data(), static_cast<size_t>(n));
            }
        }
    }

    for (int fd : fds)
        ::close(fd);
}

int main(int argc, char* argv[])
{
    if (argc != 7)
    {
        fprintf(stderr, "Usage: %s <host_ip> <port> <threads> <blocksize> <sessions> <time>\n", argv[0]);
        return 1;
    }

    const char* ip = argv[1];
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    int threads = atoi(argv[3]);
    size_t blocksize = static_cast<size_t>(atoi(argv[4]));
    int sessions = atoi(argv[5]);
    int timeout = atoi(argv[6]);

    if (threads < 1)
        threads = 1;
    if (sessions < threads)
        sessions = threads;

    Stats stats;
    std::atomic<bool> stop{false};

    std::vector<std::thread> tvec;
    int base = sessions / threads;
    int extra = sessions % threads;
    for (int t = 0; t < threads; ++t)
    {
        int nconn = base + (t < extra ? 1 : 0);
        tvec.emplace_back(worker, ip, port, blocksize, nconn, std::ref(stop), std::ref(stats));
    }

    // 运行 timeout 秒后停止
    std::this_thread::sleep_for(std::chrono::seconds(timeout));
    stop.store(true);
    for (auto& th : tvec)
        th.join();

    double mib = static_cast<double>(stats.bytes.load()) / (1024.0 * 1024.0);
    double mib_s = mib / timeout;
    printf("%lld total bytes read\n", static_cast<long long>(stats.bytes.load()));
    printf("%lld total messages read\n", static_cast<long long>(stats.messages.load()));
    printf("%.6f MiB/s throughput\n", mib_s);
    return 0;
}
