#include <cassert>
#include <liburing.h>

#include "AcceptContext.h"
#include "Acceptor.h"
#include "Logger.h"

AcceptContext::AcceptContext(sockaddr_in addr, int fd)
    :addr_(addr)
    ,fd_(fd)
    ,is_error_(false)
    ,is_accepting_(false)
    ,acceptor_(nullptr)
{
    assert(fd_&&"fd is empty ,check the logic");
}

AcceptContext::~AcceptContext()
{
}

bool AcceptContext::handleError()
{
    if(res_<0)
    {
        int err = -res_;
        switch (err)
        {
        //被取消了
        case ECANCELED :
            return true;

        //文件描述符耗尽
        case EMFILE:       
        case ENFILE:
            LOG_ERROR("Accept error: Too many open files. Pausing accept.");
            is_error_= true;
            return true;
        
        default:
            LOG_ERROR("Accept fatal error: %d", err);
            is_error_ = true;
            return true;
        }
    }
    return false;
}

void AcceptContext::on_completion()
{
    assert(is_accepting_&&"is_accepting_ should not be false, some logic is wrong");
    LOG_DEBUG("AcceptContext res:%d flags:%u",res_,flags_);

    bool need_close = false;
    //处理错误
    if(res_<0)
    {
        need_close = handleError();
    }
    else//res>=0,正常和处理连接
    {
        acceptor_->handleRead(res_);
    }

    if(!(flags_&IORING_CQE_F_MORE))
    {
        //如果是意外结束但是不致命，继续提交
        if(!need_close)
        {
            acceptor_->submitAccept();
        }
        else//如果致命，直接关闭
        {
            is_accepting_ = false;
            return;
        }
    }
}
