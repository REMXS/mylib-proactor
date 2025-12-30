#pragma once
#include <memory>
#include <coroutine>
#include <vector>
#include <sys/uio.h>

#include "IoContext.h"
#include "SendQueue.hpp"

class TcpConnection;

//返回的cqe调用的就是这个类，所以这个类要决定返回后业务协程是否唤醒等一系列操作
struct WriteContext:public IoContext
{
    std::shared_ptr<TcpConnection>holder_;//在提交sqe后保活
    std::coroutine_handle<>write_handle_;//业务协程的句柄
    SendQueue output_buffer_;//输出缓冲区
    size_t high_water_mark_;//高水位阈值
    size_t low_water_mark_; //低水位阈值

    //这里只是持有fd用于提交，并不管理这个fd，fd的管理有连接类进行管理
    int fd_;
    bool is_sending_;       //标识当前连接是否正在等待cqe中
    bool is_error_;         //是否出错要关闭连接

    size_t max_slices_;             //一次性发送的最大的slices数量
    std::vector<iovec>temp_data_;   //交给cqe发送但是还有没回来的iovec

    WriteContext(size_t high_water_mark,size_t low_water_mark,int fd,size_t max_slices =256);
    ~WriteContext();

    //批量提取数据提交数据到io_uring中
    void flush();

    //处理错误
    bool handleError();

    //cqe返回后调用的回调函数
    void on_completion()override;

    bool isError()const {return is_error_;}

    bool overLoad()const {return output_buffer_.getTotalLen()>high_water_mark_;}

    bool underLoad()const {return output_buffer_.getTotalLen()<low_water_mark_;}
    //iovec* retrieveTempData();
};