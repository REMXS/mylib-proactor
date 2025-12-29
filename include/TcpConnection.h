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


// class IoUringLoop;

// class RecvDataAwaiter;
// class SendDataAwaiter;

// class TcpConnection:noncopyable , public std::enable_shared_from_this<TcpConnection>
// {
// private:
//     IoUringLoop&io_uring_loop_;
//     Task<>task_handle_;//具体业务协程句柄
//     Socket sock_;
//     bool closing_;     //判断这个连接是否正在关闭

//     //读写处理函数，在对应的on_completion中调用
//     void handleRead();
//     void handleWrite();

//     void sendInLoop();
//     void readInLoop();

    

// public:
//     TcpConnection(/* args */);
//     ~TcpConnection();


//     ReadContext read_context_;
//     WriteContext write_context_;

//     void handleClose();

//     //发送数据
//     RecvDataAwaiter send(std::string data);
//     //接受数据
//     SendDataAwaiter read();
    

//     std::shared_ptr<TcpConnection>getSharedPtr(){return shared_from_this();}
//     inline int getFd()const {return sock_.fd();}
//     inline ReadContext* getReadContext() {return &read_context_;}
//     inline WriteContext* getWriteContext() {return &write_context_;}
// };


// class RecvDataAwaiter
// {
//     /* 
//     这里可以使用裸指针，因为业务协程不结束销毁，
//     TcpConnection就不会销毁，因为Awaiter中的指针是有效的 
//     */
//     TcpConnection* conn_;  
// };

// class SendDataAwaiter
// {
//     TcpConnection* conn_;
// };
