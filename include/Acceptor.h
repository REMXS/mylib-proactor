#pragma once

#include<functional>
#include <queue>
#include <memory>

#include "Socket.h"
#include "noncopyable.h"
#include "IoContext.h"
#include "InetAddress.h"
#include "AcceptContext.h"

class InetAddress;
class IoUringLoop;

class Acceptor :noncopyable , public std::enable_shared_from_this<Acceptor>
{
public:
    using NewConnectionCallBack=std::function<void(int sockfd,const InetAddress&)>;
private:
    friend AcceptContext;
    IoUringLoop* loop_;//listenfd 所在的loop

    //socket和channel中绑定的fd就是listenfd，fd的声明周期和socket绑定，socket析构时，fd关闭
    Socket accept_socket_; //专门用于接听新连接的socket
    InetAddress addr_;     //监听的地址 

    bool listening_;

    NewConnectionCallBack connection_callback_;//用于处理新建立连接的函数

    AcceptContext accept_context_;

    void handleRead(int connfd);//listenfd 的读回调函数

    void submitAccept();

public:
    Acceptor(IoUringLoop*loop,const InetAddress&addr,bool reuse);
    ~Acceptor();

    //设置listenfd的listen状态,并将其加入poller中进行监听
    void listen(); 
    bool isListening(){return listening_;}

    void setConnetionCallback(NewConnectionCallBack cb){connection_callback_=std::move(cb);}

    AcceptContext* getAcceptContext(){return &accept_context_;}

    int getFd()const {return accept_socket_.fd();}

    const sockaddr_in* getSockAddr()const{return addr_.getSockAddr();}

    std::shared_ptr<Acceptor>getSharedPtr(){return shared_from_this();}
};


