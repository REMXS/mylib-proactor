#pragma once
#include <memory>
#include <coroutine>

#include "IoContext.h"
#include "InputChainBuffer.h"
#include "noncopyable.h"
class TcpConnection;

struct ReadContext:public IoContext ,noncopyable
{
    //提交了sqe避免tcp connection提前析构
    std::shared_ptr<TcpConnection>holder_;
    std::coroutine_handle<>read_handle_;//业务协程的句柄
    InputChainBuffer input_buffer_; //输入缓冲区
    size_t high_water_mark_;//高水位阈值
    size_t high_water_mark_chunk_;//chunk的高水位阈值，如果占用的chunk过多也停止接收数据

    //这里只是持有fd用于提交，并不管理这个fd，fd的管理有连接类进行管理
    int fd_;
    bool is_error_;         //标识当前连接的读取是否出错

    enum class ReadStatus
    {
        STOPED,     //没有提交sqe的状态
        READING,    //已经提交sqe，正在接收cqe
        CANCELING   //因为背压提交了取消的sqe，但是cancel sqe 的cqe还没有返回
    };
    ReadStatus status_;
    
    ReadContext(size_t high_water_mark,size_t high_water_mark_chunk,int fd,ChunkPoolManagerInput&manager);
    ~ReadContext();
    
    bool handleError();

    void on_completion()override;

    inline bool overLoad()const
    {return input_buffer_.getTotalLen()>high_water_mark_||input_buffer_.getTotalChunk()>high_water_mark_chunk_;}

    inline bool isEmpty()const {return input_buffer_.getTotalLen()==0;}
};