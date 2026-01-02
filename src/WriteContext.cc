#include <cassert>
#include <errno.h>
#include "WriteContext.h"
#include "TcpConnection.h"
#include "Logger.h"

WriteContext::WriteContext(size_t high_water_mark,int fd,size_t max_slices)
    :high_water_mark_(high_water_mark)
    ,max_slices_(max_slices)
    ,fd_(fd)
    ,holder_(nullptr)
    ,write_handle_(nullptr)
    ,is_sending_(false)
    ,is_error_(false)
{
    assert(fd&&"fd is 0 , check the logic");
}

WriteContext::~WriteContext()
{
}

void WriteContext::flush()
{
    //可选：1.直接清空temp_data重新装填，2.偏移iovec数组，如果不为空继续发送，这里选择1
    temp_data_.clear();
    output_buffer_.getBatchFragment(temp_data_,max_slices_);

    assert(!temp_data_.empty()&&"temp_data_ is empty ,some logic is wrong");

    holder_->submitWrite(this);
    is_sending_ = true;
}

bool WriteContext::handleError()
{
    if(res_<0)
    {
        int err = -res_;
        LOG_INFO("WriteContext::handleError error happened, error:%s",strerror(err));
        switch (err)
        {
        //连接被对端关闭，要手动关闭连接,此时关闭操作要由写端触发
        case EPIPE:
        case ECONNRESET:
            is_error_ = true;
            if(write_handle_){
                write_handle_.resume();
            }
            //通知连接进行关闭操作
            holder_->handleClose();
            return true;
        

        //缓冲区满，暂时无法提交，但是理论上不会出现这种情况
        case EAGAIN:
        case ENOBUFS:
            return false;
        
        //被信号中断，继续进行
        //一个可能的优化：直接调用接口提交，然后直接退出，这样就省去填充temp_data的开销
        case EINTR:
            return false;

        //写入以关闭的fd，说明连接已经在其它地方关闭过了，或者是出了其它的错误
        case EBADF:
        case ENOTCONN:
            is_error_ = true;
            holder_->handleClose();
            return true;

        //其它的未知错误
        default:
            LOG_ERROR("unknown error happened error");
            is_error_ = true;
            if(write_handle_){
                write_handle_.resume();
            }
            holder_->handleClose();
            return true;
        
        }
    }

    //理论不会触发
    return true;
}

void WriteContext::on_completion()
{   

    
    LOG_DEBUG("WriteContext res : %d flags: %u",(int)res_,flags_);
    /*
    提示：这个头部的判断会破坏状态，应该在连接关闭的时候close fd，然后在处理错误的阶段关闭而不是由外部的状态关闭 
    */
    // //检查连接是否关闭，如果关闭，就直接退出
    // if(holder_->closing_)
    // {
    //     is_sending_ = false;
    //     holder_.reset();
    //     return;
    // }

    //理论上在触发on_completion函数的时候因为要保证连接的生命周期，shared_ptr中是不为空的
    assert(holder_&&"the holder should not be nullptr,some logic is wrong");

    //检查res，查看发送是否成功，如果失败就处理错误
    if(res_<0)
    {
        bool need_close = handleError();
        is_sending_ = false;
        if(need_close)
        {
            holder_.reset();
            is_sending_ = false;
            return;
        }
    }
    else//如果发送成功，获取发送完毕的数据的数量，推进缓冲区
    {
        output_buffer_.retrieve(res_);
    }

    //如果缓冲区中有数据，就发送
    if(output_buffer_.getTotalLen()>0)
    {
        flush();
    }
    else
    {
        is_sending_=false;
        //记得如果不提交请求就把这个值清零，否则连接的生命周期就会延长
        holder_.reset();
    }   

    //如果handle显示当前协程正在等待且数据量低于高水位线，就唤醒协程
    //(协程挂起是因为输出缓冲区数据太多或者是现在协程是在其它的线程中刚处理完任务)
    if(write_handle_&&!overLoad())
    {
        auto handle = write_handle_;
        write_handle_ = nullptr;
        assert(!handle.done()&&"coroutine is done,some logic is wrong");
        handle.resume();
    }
}
