#pragma once

#include <coroutine>
#include "IoContext.h"


class IoUringLoop;

class TcpConnection
{
private:
    IoUringLoop&io_uring_loop_;
public:
    TcpConnection(/* args */);
    ~TcpConnection();


    struct ReadContext:public IoContext
    {
        std::coroutine_handle<>read_handle_;
        void on_completion()override;
    }read_context_;

    struct WriteContext:public IoContext
    {
        std::coroutine_handle<>write_handle_;
        //批量提取数据提交数据到io_uring中
        void flush();
        void on_completion()override;
    }write_context_;

    void handleClose();
    
};


