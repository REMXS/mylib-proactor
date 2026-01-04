
# mylib-proactor

**mylib-proactor** 是一个基于 Linux `io_uring` 和 C++20 协程（Coroutines）构建的高性能、现代化 TCP 网络库。它实现了 **Proactor** 设计模式，利用 `io_uring` 的异步 I/O 能力和 C++20 协程的同步编码风格，旨在提供极高的吞吐量和极低的延迟，同时保持业务代码的简洁性。

## 核心特性 (Key Features)

* **真正的异步 I/O (Proactor)**: 基于 Linux 原生 `io_uring` 接口，全链路异步化（Accept, Read, Write）。
* **C++20 协程支持**: 封装 `Task<T>` 和 Awaiter，使用同步的代码逻辑编写异步网络业务，告别回调地狱（Callback Hell）。
* **IoUring 高级特性**:
* **Buffer Ring (缓冲环)**: 使用 `io_uring` 提供的 buffer ring 机制（`IORING_REGISTER_PBUF_RING`），内核自动选择缓冲区，极大减少内存分配和系统调用开销。
* **Multishot (多射)**: 支持 `IORING_RECV_MULTISHOT` 和 `multishot accept`，一次提交即可持续接收数据或连接，大幅减少 SQE 提交频率。


* **One Loop Per Thread**: 采用经典的 Reactor/Proactor 线程模型，主线程负责分发连接，IO 线程池处理读写业务。
* **高效内存管理**: 内部实现了 `ChunkPool` 和 `InputChainBuffer`，针对网络数据收发进行了零拷贝或少拷贝优化。
* **完善的基础设施**: 内置高性能日志库（Logger）、时间戳处理（Timestamp）、线程封装（Thread）等。

## 环境要求 (Requirements)

由于使用了 `io_uring` 的高级特性和 C++23 标准，本项目对环境有较高要求：

* **操作系统**: Linux
* **内核版本**: 建议 **Linux 5.19+** (推荐 6.x+)，因为使用了 `IORING_RECV_MULTISHOT` 和 Buffer Ring 等较新特性。
* **编译器**: GCC 11+ 或 Clang 14+ (需支持 C++20/23 协程标准)。
* **构建工具**: CMake 3.15+
* **依赖库**:
* `liburing` (必须安装开发包，如 `liburing-dev`)
* `gtest` (仅用于编译测试用例)



## 目录结构 (Project Structure)

```text
.
├── include/                # 头文件
│   ├── AcceptContext.h     # Accept 操作上下文
│   ├── ChunkPool.h         # 内存池定义
│   ├── IoContext.h         # 异步操作上下文基类
│   ├── IoUringLoop.h       # EventLoop 核心封装
│   ├── Task.hpp            # C++20 协程任务封装
│   ├── TcpConnection.h     # TCP 连接封装
│   ├── TcpServer.h         # TCP 服务器封装
│   └── ...
├── src/                    # 源代码
│   ├── IoUringLoop.cc      # io_uring 提交与完成处理
│   ├── TcpConnection.cc    # 连接生命周期管理
│   ├── ChunkPoolManagerInput.cc # Buffer Ring 管理
│   └── ...
├── test/                   # 测试代码
│   ├── integration_test/   # 集成测试 (Echo Server 示例)
│   └── unit_test/          # 单元测试
└── CMakeLists.txt          # 构建脚本

```

## 构建与安装 (Build)

1. **克隆仓库**:
```bash
git clone https://github.com/your-repo/mylib-proactor.git
cd mylib-proactor

```


2. **创建构建目录并编译**:
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)

```


3. **运行测试**:
```bash
./test/UnitTest         # 运行单元测试
./test/IntegrationTest  # 运行集成测试（包含简单的压力测试）

```



## 快速上手 (Quick Start)

### 编写一个 Echo Server

得益于 C++20 协程，业务逻辑非常线性且易于理解。

```cpp
#include "TcpServer.h"
#include <iostream>

// 定义业务协程
Task<> echo_server(std::shared_ptr<TcpConnection> conn)
{
    try {
        while (true) {
            // 1. 等待数据到达 (异步挂起)
            // PrepareToRead 返回可读字节数，如果连接断开或出错返回负值
            int size = co_await conn->PrepareToRead();

            if(size < 0) break; 

            // 2. 读取数据 (从输入缓冲区获取)
            std::string data = conn->read(size); 
            
            if (data.empty()) continue;

            std::cout << "Recv: " << data << std::endl;

            // 3. 发送数据 (异步挂起，直到写入完成或出错)
            co_await conn->send(data);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main()
{
    // 配置参数
    IoUringLoopParams params{
        .ring_size_ = 4096,
        .cqes_size_ = 256,
        .low_water_mark_ = 64
    };

    // 1. 创建主 EventLoop
    IoUringLoop base_loop(params);

    // 2. 创建服务器监听 8888 端口
    InetAddress addr(8888);
    TcpServer server(&base_loop, addr, "MyEchoServer", params, echo_server);

    // 3. 设置 IO 线程数 (SubLoops)
    server.setThreadNum(4);

    // 4. 启动服务器
    server.start();

    // 5. 进入事件循环
    base_loop.loop();

    return 0;
}

```

## 架构设计细节 (Architecture Details)

### 1. 内存管理与 Buffer Ring

为了最大化 `io_uring` 的性能，本项目使用了 **Buffer Ring** 特性（由 `ChunkPoolManagerInput` 实现）。

* **预注册内存**: 内存池（`ChunkPool`）在启动时申请并锁定内存（`MAP_LOCKED`），防止 Swap。
* **内核自动选择**: 提交读请求时不需要指定具体的 buffer，内核会在数据到达时从 Buffer Ring 中挑选一个空闲 Chunk 写入数据。这解决了传统 Reactor 模型中“什么时候读、读多少、分配多大内存”的难题，避免了内存浪费和频繁的 `malloc/free`。

### 2. 协程与状态机

* **Task<T>**: 自定义的协程返回对象。
* **Awaiter**: `RecvDataAwaiter` 和 `SendDataAwaiter` 负责连接协程与底层 `io_uring` 的异步操作。
* 当调用 `co_await` 时，如果数据未就绪或 socket 不可写，协程会挂起（Suspend）。
* 当 CQE（完成队列事件）返回时，`IoUringLoop` 会通过回调（`on_completion`）恢复（Resume）对应的协程。



### 3. I/O 调度

* **Proactor 模式**: 不同于 Reactor (epoll) 的“就绪通知”，`mylib-proactor` 是“完成通知”。我们提交的是“读任务”和“写任务”，内核完成后通知我们。
* **Multishot**:
* `Accept`: 一次提交 `multishot accept`，可以不断产生新的连接 fd。
* `Read`: 使用 `IORING_RECV_MULTISHOT`，只需提交一次 SQE，内核收到数据就会不断生成 CQE，无需每次读完重新提交读请求。



## 注意事项

* **Liburing 版本**: 请确保系统的 `liburing` 版本足够新，否则可能会报符号未定义错误。
* **Buffer Overflow**: 虽然有 Buffer Ring，但如果消费速度远低于生产速度，可能会触发 `ENOBUFS`。库内部实现了背压机制和高水位线（High Water Mark）控制来缓解此问题。
