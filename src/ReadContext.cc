#include <cassert>
#include <liburing.h>

#include "ReadContext.h"
#include "TcpConnection.h"
#include "Logger.h"

//注意：read cqe 的返回顺序和cancel cqe 的返回顺序是随机的，所以不能靠 cancel CQE 改状态
//可以把cancel sqe 的user_data变量置为0 ，因为loop中特殊处理了"cqe user_data 0"为忽略这个cqe的回调，表示这个cqe不携带上下文
//业务协程只能在状态为stoped的时候才可以继续提交请求

ReadContext::ReadContext(size_t high_water_mark, size_t high_water_mark_chunk, int fd,ChunkPoolManagerInput&manager)
    :IoContext(ContextType::Read)
    ,high_water_mark_(high_water_mark)
    ,high_water_mark_chunk_(high_water_mark_chunk)
    ,fd_(fd)
    ,status_(ReadStatus::STOPED)
    ,holder_(nullptr)
    ,read_handle_(nullptr)
    ,input_buffer_(manager)
    ,is_error_(false)
{
}

ReadContext::~ReadContext()
{
}

bool ReadContext::handleError()
{
    //res =0 不是错误，是连接的正常关闭，但是处理逻辑一起合并了
    if(res_==0)
    {
        is_error_ = true;
        LOG_DEBUG("Peer closed connection")
        return true;
    }
    else if(res_<0)
    {
        int err = -res_;

        switch (err)
        {
        //对端连接关闭
        case EPIPE:
        case ECONNRESET:
            is_error_ = true;
            return true;
        
        //buffer ring中的内存池资源耗尽
        case ENOBUFS:
            LOG_ERROR("Read buffer ring empty (ENOBUFS), waiting for buffers...");
            // 视为高水位线触发的暂停，不关闭连接，等待用户消费数据后归还 Buffer
            return false;
        
        //资源暂时不可用，但是理论上不会出现这种情况
        case EAGAIN:
            return false;

        //文件描述符无效
        case ENOTCONN:
        case EBADF:
            LOG_ERROR("Fatal fd error: %d", err);
            is_error_ = true;
            return true;

        //操作被取消
        case ECANCELED:
            return false;

        default:
            LOG_ERROR("unknown error happened msg: %s",strerror(err));
            is_error_ = true;
            return true;
        }
    }
    return true;
}

void ReadContext::on_completion()
{
    LOG_DEBUG("ReadContext status : %d :res: %d",(int)status_,res_);
    //理论上在触发on_completion函数的时候因为要保证连接的生命周期，shared_ptr中是不为空的
    assert(holder_&&"the holder should not be nullptr,some logic is wrong");

    //只要触发了这个函数，context 的状态一定是reading或者是canceling的
    assert((status_==ReadStatus::CANCELING||status_==ReadStatus::READING)&&"some logic is wrong");

    bool need_close = false;
    //判断res，如果小于等于0，就处理错误或者是关闭连接
    if(res_<=0)
    {
        need_close = handleError();
    }
    else
    {
        //res>0，向缓冲区追加数据
        //获取buffer ring 中内存块的编号
        uint16_t buf_id = 0;

        assert(flags_& IORING_CQE_F_BUFFER&&"read logic is not multishut, some logic is wrong");
        if(flags_& IORING_CQE_F_BUFFER)
        {
            buf_id = flags_ >> IORING_CQE_BUFFER_SHIFT;
        }
        input_buffer_.append(buf_id,res_);

        //如果输入缓冲区的数据超过了其中一个高水位线且当前的状态不为canceling，则发送cancel sqe并转换状态
        if(overLoad()&&status_!=ReadStatus::CANCELING)
        {
            LOG_DEBUG("ReadContext high water mark triggered!");
            holder_->submitCancel(this);
            status_ = ReadStatus::CANCELING;
        }
    }

    //如果multishut的cqe返回完毕，根据情况来判断是否需要重新提交
    //注意这个标志，不是IORING_CQE_F_BUF_MORE
    if(!(flags_&IORING_CQE_F_MORE))
    {
        //状态为canceling且没有致命错误，说明是因为高水位线导致的停止，停止提交，应该通知协程处理数据
        //或者是因为 ENOBUFS 导致的停止，也应该暂停提交，等待用户消费数据
        if(status_==ReadStatus::CANCELING && !need_close)
        {
            status_ = ReadStatus::STOPED;
            holder_.reset();
        }
        //没有致命错误，说明是可以继续状态的错误，继续提交。
        else if(status_==ReadStatus::READING&&!need_close)
        {
            holder_->submitRead(this);
        }
        //如果有致命错误，直接立即停止并关闭连接
        else
        {
            if(read_handle_){
                read_handle_.resume();
            }
            holder_->handleClose();
            holder_.reset();
            status_ = ReadStatus::STOPED;
            return;
        }
    }

    if(read_handle_)
    {
        auto handle = read_handle_;
        read_handle_=nullptr;
        assert(!handle.done()&&"coroutine is done,some logic is wrong");
        handle.resume();
    }
}


