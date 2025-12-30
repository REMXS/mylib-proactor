#include <cassert>
#include <liburing.h>

#include "ReadContext.h"
#include "TcpConnection.h"

//注意：只有cancel sqe 的cqe返回后res字段为CANCELED 才可以从canceling状态转换到其它的状态
//业务协程只能在状态为stoped的时候才可以继续提交请求

void ReadContext::handleClose()
{
    
}

void ReadContext::on_completion()
{
    //理论上在触发on_completion函数的时候因为要保证连接的生命周期，shared_ptr中是不为空的
    assert(holder_&&"the holder should not be nullptr,some logic is wrong");

    //只要触发了这个函数，context 的状态一定是reading或者是canceling的

    //判断res，如果小于等于0，就处理错误或者是关闭连接
    if(res_<=0)
    {
        handleClose();
        //只要状态为stoped而且返回了错误而且连接处于关闭状态,就代表该关闭连接了
        if(status_==ReadStatus::STOPED)
        {
            holder_.reset();
            return;
        }
    }
    else
    {
        //res>0，向缓冲区追加数据
        //获取buffer ring 中内存块的编号
        assert((status_==ReadStatus::CANCELING||status_==ReadStatus::READING)&&"some logic is wrong");

        uint16_t buf_id = 0;

        assert(flags_& IORING_CQE_F_BUFFER&&"read logic is not multishut, some logic is wrong");
        if(flags_& IORING_CQE_F_BUFFER)
        {
            buf_id = flags_ >> IORING_CQE_BUFFER_SHIFT;
        }
        input_buffer_.append(buf_id,res_);
    }

    //如果multishut的cqe没有返回完，直接结束，不用考虑提交读任务

    //如果输入缓冲区的数据超过了其中一个高水位线且当前的状态不为canceling，则发送cancel sqe并转换状态
    
    
}


