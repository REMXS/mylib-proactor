#pragma once

#include<arpa/inet.h>


#include"noncopyable.h"

class InetAddress;

class Socket:noncopyable
{
private:
    const int socketfd_;
public:
    explicit Socket(int socketfd);
    ~Socket();

    int fd()const{return socketfd_;}

    //用于配置监听fd的事件监听
    void listen();
    //用于监听的fd绑定地址
    void bindAddress(const InetAddress& addr);

    //接收连接并返回连接的fd
    int accept(InetAddress& peer_addr);

    //关闭fd的写端，但是fd还可以进行读操作
    void shutDownWrite();
    
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
};


