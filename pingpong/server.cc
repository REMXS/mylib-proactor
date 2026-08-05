// io_uring pingpong echo server
// 对齐 muduo examples/pingpong/server.cc 的用法:
//   pingpong_server <ip> <port> <threads>
// 行为: 收到任何数据立即原样回显 (与 muduo onMessage 中 conn->send(buf) 一致)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "IoUringLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "TcpServer.h"
#include "Task.hpp"

void printUsage(const char* prog)
{
    printf("Usage: %s <ip> <port> <threads> [discard] [-w <output_water_mark_bytes>]\n", prog);
    printf("  discard             只收不回模式 (sockperf throughput 标准用法)\n");
    printf("  -w <bytes>          输出缓冲高水位 (默认 262144 = 256KB, 调大可延缓写背压)\n");
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        printUsage(argv[0]);
        return 1;
    }

    const char* ip = argv[1];
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    int threads = atoi(argv[3]);

    // 解析可选参数: discard 开关 和 -w 输出高水位
    bool discard = false;
    size_t out_water_mark = 8192 * 32;  // 默认 256KB
    for (int i = 4; i < argc; ++i)
    {
        if (strcmp(argv[i], "discard") == 0)
            discard = true;
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
            out_water_mark = static_cast<size_t>(strtoull(argv[++i], nullptr, 10));
    }

    // io_uring 参数与 http_server 一致
    IoUringLoopParams params{8192, 2048, 1024, 8192, 4096};
    auto base_loop = std::make_unique<IoUringLoop>(params);

    InetAddress addr(port, ip);

    // echo 协程: 收到数据立即原样回显
    auto handler = [discard](TcpConnectionPtr conn) -> Task<> {
        try
        {
            while (true)
            {
                int n = co_await conn->PrepareToRead();
                if (n < 0)
                    break;

                // 循环处理缓冲区内所有数据 (粘包), 与 muduo 一致: 收到多少回多少
                while (true)
                {
                    auto [buf, len] = conn->peek();
                    if (len == 0)
                        break;
                    if (!discard)
                    {
                        co_await conn->send(std::string(buf, len));
                    }
                    conn->retrieve(len);
                }
            }
        }
        catch (const std::exception&)
        {
            // 连接关闭等异常, 协程结束即销毁连接
        }
    };

    TcpServer server(base_loop.get(), addr, "pingpong", params, std::move(handler));
    server.setThreadNum(threads);
    // 只调输出高水位 (输入高水位保持默认, 与输出解耦)
    server.setHighWaterMarks(8192 * 32, 32, out_water_mark);
    server.start();

    LOG_INFO("pingpong_server listening on %s:%u, threads=%d, out_water_mark=%zu",
             ip, port, threads, out_water_mark);

    base_loop->loop();
    return 0;
}
