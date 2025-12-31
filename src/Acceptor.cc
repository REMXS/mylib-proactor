#include<arpa/inet.h>
#include<errno.h>

#include "Acceptor.h"
#include "Logger.h"
#include "Timestamp.h"
#include "InetAddress.h"
#include "IoUringLoop.h"


//创建一个非阻塞的fd
static int createNonBlocking()
{
    int fd=socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,IPPROTO_TCP);
    if(fd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create error",__FILE__,__FUNCTION__,__LINE__)
    }
    return fd;   
}


Acceptor::Acceptor(IoUringLoop*loop,const InetAddress&addr,bool reuse)
    :loop_(loop)
    ,listening_(false)
    ,accept_socket_(createNonBlocking())
    ,accept_context_(*addr.getSockAddr(),accept_socket_.fd())
{
    accept_socket_.setReuseAddr(true);
    accept_socket_.setReusePort(reuse);
    accept_socket_.bindAddress(addr);
}


Acceptor::~Acceptor()
{
}

void Acceptor::listen()
{
    listening_=true;
    accept_socket_.listen();

    accept_context_.acceptor_ = this;
    accept_context_.is_accepting_ = true;
    submitAccept();
}

void Acceptor::handleRead(int connfd)
{

    if(connfd>0)
    {
        //获取对端地址
        sockaddr_in conn_addr;
        socklen_t len = sizeof(sockaddr_in);
        getpeername(connfd,(sockaddr*)&conn_addr,&len);
        InetAddress peer_addr(conn_addr);
        if(connection_callback_)
        {
            connection_callback_(connfd,peer_addr);
        }
        else
        {
            ::close(connfd);//如果没有对新连接处理的函数，则关闭新的连接
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept error:%d",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno==EMFILE)
        {
            LOG_ERROR("%s:%s:%d socket fd reached limit",__FILE__,__FUNCTION__,__LINE__)
        }
    }
}

void Acceptor::submitAccept()
{
    return loop_->submitAcceptMultishut(&accept_context_);
}
