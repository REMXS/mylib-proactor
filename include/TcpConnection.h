#pragma once

#include <memory>
#include <coroutine>
#include <vector>

#include "IoContext.h"
#include "Task.hpp"
#include "noncopyable.h"
#include "Socket.h"
#include "ReadContext.h"
#include "WriteContext.h"
#include "Callbacks.h"
#include "InetAddress.h"


class IoUringLoop;

class RecvDataAwaiter;
class SendDataAwaiter;

class TcpConnection:noncopyable , public std::enable_shared_from_this<TcpConnection>
{
private:
    friend WriteContext;
    friend ReadContext;
    friend RecvDataAwaiter;
    friend SendDataAwaiter;

    const std::string name_;    //这个连接的名字

    //管理整个连接的loop
    IoUringLoop&loop_;
    Task<>task_handle_;//具体业务协程句柄
    Socket sock_;
    bool closing_;     //判断这个连接是否正在关闭

    //关闭连接的回调函数
    CloseCallback close_callback_;

    const InetAddress local_addr_;//本机地址
    const InetAddress peer_addr_;//这个连接中对方的地址

    ReadContext read_context_;
    WriteContext write_context_;

    //处理连接关闭的清理操作
    void handleClose(); 

    void sendInLoop(std::string data);

    void submitWrite(WriteContext* w_ctx);
    void submitRead(ReadContext* r_ctx);
    //向内核提交取消read sqe的请求，因为read sqe 时multishut，不主动取消不会停止
    void submitCancel(ReadContext* r_ctx);

public:
    TcpConnection(
        std::string name,
        IoUringLoop&loop,
        int sockfd,
        InetAddress local_addr,
        InetAddress peer_addr,
        size_t input_high_water_mark,
        size_t input_high_water_mark_chunk,
        size_t out_put_high_water_mark
    );
    
    ~TcpConnection();

    //发送数据
    SendDataAwaiter send(std::string data);


    //准备读取数据
    RecvDataAwaiter PrepareToRead();
    //读取数据
    std::string read(size_t size);
    //只查看数据
    std::pair<char *,size_t>peek();
    //手动偏移数据
    void retrieve(size_t size);
    

    std::shared_ptr<TcpConnection>getSharedPtr(){return shared_from_this();}
    inline int getFd()const {return sock_.fd();}
    inline ReadContext* getReadContext() {return &read_context_;}
    inline WriteContext* getWriteContext() {return &write_context_;}
    const std::string getName()const {return name_;}

    void setCloseCallback(CloseCallback cb){close_callback_ = std::move(cb);}

    void Established(Task<>task_handle);

    std::string name()const {return name_;}

    IoUringLoop* getLoop()const {return &loop_;}

    void Destroyed() {handleClose();}
};


class RecvDataAwaiter
{
private:
    /* 
    这里可以使用裸指针，因为业务协程不结束销毁，而业务协程中传入的是智能指针
    TcpConnection就不会销毁，因为Awaiter中的指针是有效的 
    */
    TcpConnection* conn_;  
public:
    RecvDataAwaiter(TcpConnection* conn)
        :conn_(conn)
    {}
    ~RecvDataAwaiter()= default;
    bool await_ready();

    void await_suspend(std::coroutine_handle<>h);

    int await_resume();
};

class SendDataAwaiter
{
private:
    TcpConnection* conn_;
    std::string data_;
public:
    SendDataAwaiter(TcpConnection* conn,std::string data)
        :conn_(conn)
        ,data_(std::move(data))
    {}
    ~SendDataAwaiter()=default;

    bool await_ready();

    void await_suspend(std::coroutine_handle<>h);

    bool await_resume();
};
