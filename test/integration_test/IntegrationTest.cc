#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <gtest/gtest.h>

#include "TcpServer.h"

Task<> echo_server(std::shared_ptr<TcpConnection> conn)
{
    try {
        while (true) {
            // 1. 等待数据到达
            int size = co_await conn->PrepareToRead();

            //如果读取数据出错，就直接退出
            if(size<0) break;
            // 2. 读取数据
            // 这里简单读取所有可用数据，实际业务可能需要处理粘包
            std::string data = conn->read(size); 
            
            if (data.empty()) {
                std::cout << "empty data" << std::endl;
                continue;
            }

            std::cout << "Recv from [" << conn->getName() << "]: " << data.size() << std::endl;

            // 3. 发送数据 (Echo)
            co_await conn->send(data);
        }
    } catch (const std::exception& e) {
        std::cout << "Exception in coroutine: " << e.what() << std::endl;
    }
}


class IntegrationTest:public ::testing::Test
{
protected:
    
    static void SetUpTestSuite()
    {
        InetAddress addr(test_port);
        base_loop = std::make_unique<IoUringLoop>(params);
        tcp_server=std::make_unique<TcpServer>(base_loop.get(),addr,"server1",params,echo_server);
        tcp_server->setThreadNum(4);

        tcp_server->start();
    }

    static void TearDownTestSuite()
    {
        tcp_server.reset();
        base_loop.reset();
    }

    void SetUp()override
    {

    }

    void TearDown()override
    {

    }

    inline static uint16_t test_port=9999;
    inline static IoUringLoopParams params{4096,256,64};
    inline static std::unique_ptr<IoUringLoop>base_loop;
    inline static std::unique_ptr<TcpServer>tcp_server;
};


void simpleStressTest(IoUringLoop* base_loop)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for(int num=0;num<15;++num)
    {
        std::vector<int>sock_fds(1000,0);
        //创建连接
        for(auto&n:sock_fds)
        {
            n=socket(AF_INET,SOCK_STREAM,0);
            assert(n&&"failed to create a socket");
            int optval=1;
            ::setsockopt(n,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
            ::setsockopt(n,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof(optval));
        }
        //绑定地址，发送信息
        sockaddr_in addr;
        socklen_t addr_len=sizeof(addr);
        addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=inet_addr("127.0.0.1");
        addr.sin_port=htons(9999);
        std::string data="hello world";
        for(auto&n:sock_fds)
        {
            int ret=connect(n,(sockaddr*)&addr,addr_len);
            if(ret==-1) perror("connect failed");
            assert(ret>=0&&"connect failed");
            write(n,data.c_str(),data.size());
        }

        //读取数据，验证数据的正确性
        for(auto&n:sock_fds)
        {
            char buf[32]={0};
            read(n,buf,32);
            ASSERT_EQ(data,buf);
        }

        //关闭连接
        for(auto&n:sock_fds)
        {
            ::close(n);
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    base_loop->quit();
}

TEST_F(IntegrationTest,BaseTest)
{
    std::thread t(simpleStressTest,base_loop.get());

    base_loop->loop();
    t.join();
}

