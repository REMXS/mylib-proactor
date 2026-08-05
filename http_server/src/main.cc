#include <csignal>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

#include "IoUringLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "TcpServer.h"

#include "HttpServer.h"

namespace {

void ignoreSigPipe()
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, nullptr);
}

void printUsage(const char* prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("  -p, --port <port>    listen port (default: 8080)\n");
    printf("  -t, --threads <n>    sub-loop thread count (default: 4)\n");
    printf("  -b, --body <bytes>   response body size in bytes (default: 0)\n");
    printf("  -h, --help           show this help\n");
}

} // namespace

int main(int argc, char* argv[])
{
    uint16_t port = 8080;
    int threads = 4;
    size_t body_size = 0;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--port")
        {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "-t" || arg == "--threads")
        {
            threads = std::stoi(argv[++i]);
        }
        else if (arg == "-b" || arg == "--body")
        {
            body_size = std::stoull(argv[++i]);
        }
        else if (arg == "-h" || arg == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "unknown option: %s\n", arg.c_str());
            printUsage(argv[0]);
            return 1;
        }
    }

    ignoreSigPipe();

    // io_uring 队列参数（调大以支撑更高并发/吞吐）
    // ring_size 4096->8192:  SQE 队列翻倍
    // cqes     1024->2048:  每轮批量消费更多 CQE
    // low_water 256->1024:  ring 的 1/8, 背压更晚触发(对大 body 友好)
    // chunk_size 4096->8192: 每块 8KB, 4KB body 一个 chunk 即可容纳
    // chunk_num  1024->4096: 块数再翻倍(2 的幂), 每线程内存池 32MB
    IoUringLoopParams params{8192, 2048, 1024, 8192, 4096};
    auto base_loop = std::make_unique<IoUringLoop>(params);

    InetAddress addr(port, "0.0.0.0");

    // 业务协程：解析 HTTP 请求并回固定响应（Keep-Alive）
    auto handler = [body_size](TcpConnectionPtr conn) -> Task<> {
        try
        {
            std::string body(body_size, 'x');
            char header[128];
            int header_len = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\n"
                                      "Content-Length: %zu\r\n"
                                      "Connection: keep-alive\r\n\r\n",
                                      body_size);

            if (header_len > 0)
            {
                while (true)
                {
                    // 等待数据到达（multishot recv 唤醒）
                    int n = co_await conn->PrepareToRead();
                    if (n < 0)
                        break;

                    // 循环处理粘包：缓冲区中可能包含多个请求
                    while (true)
                    {
                        auto [buf, len] = conn->peek();
                        if (len == 0)
                            break;

                        size_t total = http_bench::parseRequest(buf, len);

                        // 半包：头部不完整，或 body 未到齐，等待更多数据
                        if (total == 0 || total > len)
                            break;

                        // 解析失败：回 400 并关闭连接
                        if (total == std::numeric_limits<size_t>::max())
                        {
                            co_await conn->send(http_bench::badRequestResponse());
                            conn->retrieve(len);
                            break;
                        }

                        // 构建响应（header + body）
                        std::string resp;
                        resp.reserve(static_cast<size_t>(header_len) + body.size());
                        resp.append(header, static_cast<size_t>(header_len));
                        resp.append(body);

                        co_await conn->send(std::move(resp));

                        // 消费掉这个请求
                        conn->retrieve(total);
                    }
                }
            }
        }
        catch (const std::exception&)
        {
            // 连接关闭等异常，协程结束即销毁连接
        }
    };

    TcpServer server(base_loop.get(), addr, "http_server", params, std::move(handler));
    server.setThreadNum(threads);
    server.start();

    LOG_INFO("http_server listening on 0.0.0.0:%u, threads=%d, body_size=%zu",
             port, threads, body_size);

    base_loop->loop();
    return 0;
}
