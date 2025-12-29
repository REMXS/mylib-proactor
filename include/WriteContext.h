#pragma once
#include <memory>
#include <coroutine>
#include <vector>
#include <sys/uio.h>

#include "IoContext.h"

class TcpConnection;
struct WriteContext:public IoContext
{
    std::shared_ptr<TcpConnection>holder_;
    std::coroutine_handle<>write_handle_;
    size_t high_water_mark_;//高水位阈值
    size_t low_water_mark_; //低水位阈值
    int fd_;

    std::vector<iovec>temp_data_;   //交给cqe发送但是还有没回来的iovec
    

    //批量提取数据提交数据到io_uring中
    void flush(){};
    void on_completion()override{};
};