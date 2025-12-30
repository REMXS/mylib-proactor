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

    //管理整个连接的loop
    IoUringLoop&loop_;
    Task<>task_handle_;//具体业务协程句柄
    Socket sock_;
    bool closing_;     //判断这个连接是否正在关闭

    //处理连接关闭的清理操作
    void handleClose(); 
    

    void sendInLoop(std::string data);
    void readInLoop();

    void submitWrite(WriteContext* w_ctx);
    void submitRead(ReadContext* r_ctx);

public:
    TcpConnection(/* args */);
    ~TcpConnection();


    ReadContext read_context_;
    WriteContext write_context_;

    void handleClose();

    //发送数据
    SendDataAwaiter send(std::string data);
    //接受数据
    RecvDataAwaiter read();
    

    std::shared_ptr<TcpConnection>getSharedPtr(){return shared_from_this();}
    inline int getFd()const {return sock_.fd();}
    inline ReadContext* getReadContext() {return &read_context_;}
    inline WriteContext* getWriteContext() {return &write_context_;}
};


class RecvDataAwaiter
{
    /* 
    这里可以使用裸指针，因为业务协程不结束销毁，而业务协程中传入的是智能指针
    TcpConnection就不会销毁，因为Awaiter中的指针是有效的 
    */
    TcpConnection* conn_;  


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

    bool await_ready();

    void await_suspend(std::coroutine_handle<>h);

    bool await_resume();
};
