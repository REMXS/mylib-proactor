#pragma once
#include <memory>
#include <coroutine>

class TcpConnection;
#include "IoContext.h"

struct ReadContext:public IoContext
{
    //提交了sqe避免tcp connection提前析构
    std::shared_ptr<TcpConnection>holder_;
    std::coroutine_handle<>read_handle_;
    size_t high_water_mark_;//高水位阈值
    size_t low_water_mark_; //低水位阈值 
    int fd_;
    
    void on_completion()override{};
};