#include <unistd.h>
#include <cstring>
#include<netinet/tcp.h>

#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"


#include"Socket.h"

Socket::Socket(int socketfd)
    :socketfd_(socketfd)
{
}

Socket::~Socket()
{
    ::close(socketfd_);
}

void Socket::listen()
{
    if(::listen(socketfd_,1024*16)!=0)
    {
        LOG_FATAL("listen socket fd %d failed",socketfd_)
    }
}

void Socket::bindAddress(const InetAddress& addr)
{
    socklen_t len=sizeof(sockaddr_in);
    if(::bind(socketfd_,(sockaddr*)addr.getSockAddr(),len)<0)
    {
        LOG_FATAL("bind socket fd %d failed",socketfd_)
    }
}



int Socket::accept(InetAddress& peer_addr)
{
    socklen_t len=sizeof(sockaddr_in);
    sockaddr_in addr;
    ::memset(&addr,0,sizeof(addr));

    //这里设置接收的连接为非阻塞读写和在执行exec函数时关闭
    int connfd = accept4(socketfd_,(sockaddr*)&addr,&len,SOCK_NONBLOCK|SOCK_CLOEXEC);

    if(connfd>=0)
    {
        peer_addr.setSockAddr(addr);
    }

    return connfd;
}

void Socket::shutDownWrite()
{
    if(::shutdown(socketfd_,SHUT_WR)!=0)
    {
        LOG_ERROR("socket fd %d shut down failed",socketfd_)
    }
}


/*  这个选项用来控制是否启用 Nagle 算法 (Nagle's algorithm)。
默认情况 (off / false): Nagle 算法是启用的。它的工作方式是：当程序要发送少量数据时（比如小于一个 MSS，最大分段大小），
TCP 不会立即发送，而是会等待一小段时间。它期望能收集更多的数据，然后将它们合并成一个更大的包再发送出去，或者等到收到上
一个已发送数据的 ACK 确认后再发送。这样做的好处是减少了网络中 "小碎包" 的数量，提高了网络整体吞吐量，降低了网络拥塞的
风险。但缺点是牺牲了实时性，带来了延迟。启用 TCP_NODELAY (on / true): 禁用 Nagle 算法。这意味着任何 write 或 send操
作都会立即将数据包发送出去，不管数据包有多小。 
*/
void Socket::setTcpNoDelay(bool on)
{
    int optval=on?1:0;
    setsockopt(socketfd_,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof(optval));
}

/* 这个选项主要解决服务器重启时 bind 失败的问题。
当一个 TCP 连接正常关闭时，主动关闭方（通常是服务器）的 socket 会进入一个名为 TIME_WAIT 的状态，并持续一段时间
（通常是2*MSL，即 1-4 分钟）。在这个状态期间，操作系统会保留这个 IP:端口 组合，不允许任何新的 socket 绑定到它上面。

问题: 如果服务器程序崩溃或重启，它会尝试立即重新 bind 到原来的端口。但由于该端口可能还处于 TIME_WAIT 状态，
bind 调用会失败，导致服务器无法启动，必须等待几分钟。
启用 SO_REUSEADDR (on / true): 允许你的程序 bind 到一个仍处于 TIME_WAIT 状态的端口。这使得服务器可以立即重启并恢复服务。
*/
void Socket::setReuseAddr(bool on)
{
    int optval=on?1:0;
    setsockopt(socketfd_,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
}

/* 这是一个比 SO_REUSEADDR 更强大的选项。

启用 SO_REUSEPORT (on / true): 允许多个进程或多个线程中的不同 socket 同时 bind 到完全相同的 IP:端口 组合上，只要所有这些 
socket 在 bind 之前都设置了 SO_REUSEPORT 选项。

当多个 socket 监听同一个端口时，内核会自动在这些 socket 之间进行负载均衡，将新来的连接请求（SYN 包）分发给其中一个 socket。
*/
void Socket::setReusePort(bool on)
{
    int optval=on?1:0;
    setsockopt(socketfd_,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof(optval));
}

/* 这个选项用于检测 "僵尸连接" 或 "死连接"。
默认情况 (off / false): 如果 TCP 连接的双方都没有数据交换，那么这个连接会一直存在，即使其中一方已经断电、断网或崩溃。
另一方对此一无所知，会一直认为连接是有效的，这会占用服务器资源。启用 SO_KEEPALIVE (on / true): 当一个连接在一段时间
内（例如，默认2小时）没有任何数据通信时，TCP 协议栈会自动发送一个 "保活探测包" (Keep-Alive Probe) 给对端。如果收到对
端的正常响应，说明连接还活着，计时器重置。如果多次发送探测包后仍未收到任何响应，内核就会认为该连接已断开，并通知应用程
序（例如，read 会返回错误），服务器就可以关闭这个无效连接，释放资源。 
*/
void Socket::setKeepAlive(bool on)
{
    int optval=on?1:0;
    setsockopt(socketfd_,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof(optval));
}
/*
注意：SOL_SOCKET (Socket Level) 层的选项被设计为协议无关的通用选项。它们代表了一些对任何类型的 socket 都可能适用的通用概念。
SO_KEEPALIVE 在 API 设计者眼中，代表的是一个通用的概念：“请为这个面向连接的 socket 启用一个保活机制”。设计者并不关心底层具
体是哪个协议（TCP, SCTP, ...）以及它如何实现这个保活机制。API 只提供一个统一的“开关”。 
*/