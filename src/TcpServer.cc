#include <cassert>
#include <arpa/inet.h>
#include <cstring>

#include "TcpServer.h"
#include "Logger.h"


TcpServer::TcpServer(IoUringLoop *base_loop, InetAddress bind_addr,
                     std::string name,IoUringLoopParams loop_params,
                     CoroutineHandler coroutine_handler,Option reuse_option) 
                    :loop_(base_loop)
                    ,ip_port(bind_addr.toIpPort())
                    ,name_(name)
                    ,num_threads_(0)
                    ,started_(0)
                    ,next_conn_id_(1)
                    ,coroutine_handler_(std::move(coroutine_handler))
                    ,acceptor_(std::make_unique<Acceptor>(loop_,bind_addr,reuse_option==kReusePort))
                    ,pool_(std::make_shared<IoUringLoopThreadPool>(base_loop,name,loop_params))
{
    acceptor_->setConnetionCallback([this](int sock_fd,const InetAddress&peer_addr){
        newConnection(sock_fd,peer_addr);
    });
}

TcpServer::~TcpServer() 
{
    for(auto&[_,conn_ptr]:connection_map_)
    {
        auto tem_conn=conn_ptr;
        //清除表中的连接
        conn_ptr.reset();
        //在对应的loop中删除连接
        IoUringLoop* ioloop=tem_conn->getLoop();
        ioloop->queueInLoop([conn=tem_conn](){conn->Destroyed();});
    }
}

void TcpServer::start() 
{
    //避免重复多次启动
    if(started_.fetch_add(1)==0)
    {
        pool_->start(thread_init_callback_);

        //在baseloop中加入acceptor，启用监听
        loop_->runInLoop([this](){acceptor_->listen();});
    }
}

void TcpServer::setThreadNum(int thread_num) 
{
    assert(!started_&&"the server has already started!");
    num_threads_=thread_num;
    pool_->setNumThreads(thread_num);
}


void TcpServer::newConnection(int sock_fd, InetAddress const &peer_addr)
{
    //获取这个连接要加入的reactor
    IoUringLoop* ioloop=pool_->getNextLoop();
    
    char buf[64]={0};
    snprintf(buf,sizeof(buf),"-%s#%d",ip_port.c_str(),next_conn_id_);
    //只用存在acceptor的loop线程才会使用这个变量，所以不存在线程安全问题
    next_conn_id_++;

    std::string conn_name=name_+buf;

    LOG_INFO("%s [%s] new connection [%s] from %s",
        __FUNCTION__,name_.c_str(),conn_name.c_str(),peer_addr.toIpPort().c_str())

    //获取这个连接在本地的ip和端口
    sockaddr_in local_addr;
    ::memset(&local_addr,0,sizeof(sockaddr_in));
    socklen_t addr_len=sizeof(local_addr);

    if(::getsockname(sock_fd,(sockaddr*)&local_addr,&addr_len)<0)
    {
        LOG_ERROR("%s fd=%d failed to get local ip port",__FUNCTION__,sock_fd)
    }
    InetAddress local_a(local_addr);

    
    //建立新连接
    TcpConnectionPtr new_conn=std::make_shared<TcpConnection>(conn_name,*ioloop,sock_fd,local_a,peer_addr,4096*16,16,4096*16);
    
    //绑定回调函数
    new_conn->setCloseCallback([this](const TcpConnectionPtr&conn){removeConnection(conn);});

    //TODO 暂时没有回调函数，以后会有的
    // new_conn->setConnecitonCallback(connection_callback_);
    // new_conn->setWriteCompleteCallback(write_complete_callback_);
    
    //将新连接加入到server的表中
    connection_map_[conn_name]=new_conn;

    //协程一开始是挂起的,让对应的loop启动这个连接
    ioloop->queueInLoop([this,conn=new_conn](){conn->Established(coroutine_handler_(conn));});
}

void TcpServer::removeConnection(const TcpConnectionPtr &tcp_connection_ptr) 
{
    //在主线程进行清理操作，因为connection_map_是主线程持有的变量，只能是主线程串行访问
    loop_->runInLoop([conn=tcp_connection_ptr,this](){
        removeConnectionInLoop(conn);
    });
}

void TcpServer::removeConnectionInLoop(
    TcpConnectionPtr const &tcp_connection_ptr) 
{
    LOG_INFO("%s [%s] - connection %s",__FUNCTION__,name_.c_str(),tcp_connection_ptr->name().c_str())
    
    //从本地清理连接的信息
    connection_map_.erase(tcp_connection_ptr->name());
}
