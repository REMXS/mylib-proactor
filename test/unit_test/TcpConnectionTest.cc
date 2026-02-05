#include "test_helper.h"

#include <coroutine>
#include <thread>
#include <future>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "TcpConnection.h"
#include "Acceptor.h"
#include "IoUringLoop.h"


Task<> echo_server(std::shared_ptr<TcpConnection> conn)
{
    try {
        while (true) {
            // 1. 等待数据到达
            int size = co_await conn->PrepareToRead();

            //如果读取数据出错，就直接退出
            if(size<0)
            {
                break;
            }
            // 2. 读取数据
            // 这里简单读取所有可用数据，实际业务可能需要处理粘包
            std::string data = conn->read(size); 
            //模拟慢消费，这个测试跑的太慢了，以后放到单独一个测试程序中
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (data.empty()) {
                std::cout << "Connection closed by peer or empty read." << std::endl;
                //BUG FIX
                //break;
            }

            std::cout << "Recv from [" << conn->getName() << "]: " << data.size() << std::endl;

            // 3. 发送数据 (Echo)
            bool need_continue = co_await conn->send(data);
            if(!need_continue) break;
        }
    } catch (const std::exception& e) {
        std::cout << "Exception in coroutine: " << e.what() << std::endl;
    }
}

class TcpConnectionTest: public ::testing::Test
{
protected:
    void SetUp()override
    {
        std::promise<void> p;
        auto f = p.get_future();

        loop_thread = std::make_unique<std::thread>([&, p = std::move(p)]() mutable {
            // 1. 初始化 Loop
            // ring_size=1024, cqes_size=32 
            loop = std::make_unique<IoUringLoop>(1024,32,1,4096,32);
            // 2. 监听地址
            InetAddress listenAddr(port_);
            acceptor = std::make_unique<Acceptor>(loop.get(), listenAddr, true);
            // 3. 设置连接回调
            acceptor->setConnetionCallback([&](int sockfd, const InetAddress& peerAddr) {
                InetAddress localAddr(0); // 简化，实际可以通过 getsockname 获取
                
                std::string name = "Conn-" + std::to_string(sockfd);
                
                // 创建连接对象
                auto conn = std::make_shared<TcpConnection>(
                    name,
                    *loop,
                    sockfd,
                    localAddr,
                    peerAddr,
                    4096 * 16, // input high water mark
                    16,        // input chunk high water mark
                    1024 * 1024  // output high water mark
                );

                // 设置关闭回调（可选，用于清理全局 map 等，这里仅打印）
                conn->setCloseCallback([](const std::shared_ptr<TcpConnection>& c) {
                    std::cout << "Connection closed: " << c->getName() << std::endl;
                });

                // 启动业务协程并绑定到连接
                conn->Established(echo_server(conn));

                std::cout << "New connection: " << name << std::endl;
                //这里要把conn临时存储到一个容器中，否则在业务协程销毁时conn也会一起销毁，
                //这样会造成访问已释放内存的问题
                conns.emplace_back(conn);
            });

            acceptor->listen();
            std::cout << "Echo Server listening on 8888..." << std::endl;

            p.set_value();
            //开始循环
            loop->loop();
        });

        //确保上面的线程可以执行到循环
        f.wait();
    }

    void TearDown()override
    {
        if(loop) loop->quit();
        if(loop_thread && loop_thread->joinable())
        {
            loop_thread->join();
        } 
        acceptor.reset();
        loop.reset();
        loop_thread.reset();
        conns.clear();
        port_++;
    }


    std::unique_ptr<Acceptor> acceptor;
    std::unique_ptr<IoUringLoop> loop;
    std::unique_ptr<std::thread> loop_thread;
    std::vector<std::shared_ptr<TcpConnection>>conns;
    inline static uint16_t port_ = 8888;
};

TEST_F(TcpConnectionTest, EchoFunctionality)
{
    // 1. 创建客户端 Socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GT(client_fd, 0);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 2. 连接服务器
    int ret = -1;
    for(int i=0; i<20; ++i) {
        ret = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if(ret == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_EQ(ret, 0) << "Failed to connect to server";

    // 3. 发送数据
    std::string msg = "Hello, IoUring!";
    ssize_t sent = ::send(client_fd, msg.c_str(), msg.size(), 0);
    ASSERT_EQ(sent, msg.size());

    // 4. 接收数据
    char buf[1024];
    ssize_t received = ::recv(client_fd, buf, sizeof(buf), 0);
    ASSERT_GT(received, 0);
    std::string response(buf, received);
    
    // 5. 验证
    EXPECT_EQ(response, msg);

    // 6. 关闭
    EXPECT_EQ(close(client_fd),0);
    //这里要等待一段时间让连接关闭，不然会造成内存泄露
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 7. 停止 Loop (在 TearDown 中也会调用，但这里显式调用更清晰)
    if(loop) loop->quit();
}

TEST_F(TcpConnectionTest, LargeDataEcho)
{
    // 1. 创建客户端
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GT(client_fd, 0);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 2. 连接
    int ret = -1;
    for(int i=0; i<20; ++i) {
        ret = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if(ret == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_EQ(ret, 0) << "Failed to connect";

    // 3. 准备大数据 (4MB)
    const size_t data_size = 4 * 1024 * 1024;
    std::string send_data(data_size, 'A');
    // 填充一些随机字符以验证完整性
    for(size_t i=0; i<data_size; i+=1024) {
        send_data[i] = (char)('A' + (i % 26));
    }

    // 4. 启动发送线程
    std::thread sender([&]() {
        size_t sent = 0;
        while(sent < data_size) {
            ssize_t n = ::send(client_fd, send_data.c_str() + sent, data_size - sent, 0);
            if(n <= 0) break;
            sent += n;
        }
    });

    // 5. 接收并验证
    std::string recv_data;
    recv_data.reserve(data_size);
    char buf[65536];
    while(recv_data.size() < data_size) {
        ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
        if(n > 0) {
            recv_data.append(buf, n);
        } else if (n == 0) {
            // 对端关闭
            std::cout << "Client recv EOF" << std::endl;
            break;
        } else {
            // 出错
            if (errno == EINTR) continue;
            std::cout << "Client recv error: " << strerror(errno) << std::endl;
            break;
        }
    }

    sender.join();

    // 6. 验证
    ASSERT_EQ(recv_data.size(), data_size);
    EXPECT_EQ(recv_data, send_data);

    EXPECT_EQ(close(client_fd),0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if(loop) loop->quit();
}



