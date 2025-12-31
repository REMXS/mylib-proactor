#include <sys/eventfd.h>
#include <cstring>
#include <cassert>


#include "IoUringLoop.h"
#include "Logger.h"
#include "ChunkPoolManagerInput.h"
#include "ReadContext.h"
#include "WriteContext.h"
#include "AcceptContext.h"

//防止一个线程创建多个eventloop
//因为这个变量仅供内部判断使用，所以定义在实现文件，不对外暴露
//对外使用new 创建的eventloop对象而不使用t_loopInThisThread
thread_local IoUringLoop* t_loopInThisThread=nullptr;

// 定义默认的Poller IO复用接口的超时时间
const timespec kPollTimeS={10,0}; //10s


/*  创建一个eventfd
    EFD_NONBLOCK 表示设置为非阻塞
    EFD_CLOEXEC 表明在执行exec函数创建新进程的时候，不继承此fd
    因为eventfd用于进程中线程之间的通信，创建新进程线程的上下文不存在，
    所以eventfd要关闭防止意外的行为
 */
int createEventFd()
{
    int evtfd=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(evtfd<0)
    {
        LOG_FATAL("failed to create eventfd,errno: %d",errno)
    }
    LOG_DEBUG("successed to create eventfd %d",evtfd)
    return evtfd;
}

void IoUringLoop::doingSubmitWaitingTask()
{
    size_t processed = 0;
    const size_t max_process_per_loop = 100;  // 每轮最多处理 100 个
    
    while (!waiting_submit_queue_.empty() && 
           remainedSqe() > sqe_low_water_mark_ &&
           processed < max_process_per_loop) {
        
        auto& [ctx, submit_type] = waiting_submit_queue_.front();
        
        //处理任务
        switch (submit_type) {
            case SubmitType::ReadMultishut:
                _submitReadMultishut(ctx);
                break;
            case SubmitType::WriteMsg:
                _submitWriteMsg(ctx);
                break;
            case SubmitType::AcceptMultishut:
                _submitAcceptMultishut(ctx);
                break;
        }
        
        waiting_submit_queue_.pop();
        processed++;
    }
    
    if (processed > 0) {
        LOG_DEBUG("Processed %zu queued submit requests", processed);
    }
}

void IoUringLoop::_submitReadMultishut(IoContext* ctx)
{
    //这里理论上sqe是不为nullptr的
    auto sqe = getIoUringSqe(false);
    assert(sqe&&"the sqe should not be nullptr");

    auto read_ctx = dynamic_cast<ReadContext*>(ctx);
    assert(read_ctx&&"dynamic_cast failed,check the upper layer logic");

    io_uring_prep_read_multishot(sqe,read_ctx->fd_,CHUNK_SIZE,0,0);
    sqe->buf_group = input_chunk_manager_->get_buf_group_id();
    
    io_uring_sqe_set_data(sqe,read_ctx);
    io_uring_sqe_set_flags(sqe,IOSQE_BUFFER_SELECT);
}

void IoUringLoop::_submitWriteMsg(IoContext* ctx)
{
    //这里理论上sqe是不为nullptr的
    auto sqe = getIoUringSqe(false);
    assert(sqe&&"the sqe should not be nullptr");

    auto write_ctx = dynamic_cast<WriteContext*>(ctx);
    assert(write_ctx&&"dynamic_cast failed,check the upper layer logic");

    msghdr msg{0};
    msg.msg_iov=write_ctx->temp_data_.data();
    msg.msg_iovlen=write_ctx->temp_data_.size();

    io_uring_prep_sendmsg(sqe,write_ctx->fd_,&msg,0);
    io_uring_sqe_set_data(sqe,write_ctx);
}

void IoUringLoop::_submitAcceptMultishut(IoContext* ctx)
{
    //这里理论上sqe是不为nullptr的
    auto sqe = getIoUringSqe(false);
    assert(sqe&&"the sqe should not be nullptr");

    auto accept_ctx = dynamic_cast<AcceptContext*>(ctx);
    assert(accept_ctx&&"dynamic_cast failed,check the upper layer logic");

    //这里不能是局部变量
    static socklen_t len = sizeof(sockaddr); 
    io_uring_prep_multishot_accept(sqe,accept_ctx->fd_,(sockaddr*)&accept_ctx->addr_,&len,0);
    io_uring_sqe_set_data(sqe,ctx);
}

IoUringLoop::IoUringLoop(size_t ring_size, size_t cqes_size, size_t low_water_mark)
    :ring_(new io_uring{})
    ,cqes_(cqes_size)
    ,looping_(false)
    ,quit_(false)
    ,time_out_(kPollTimeS)
    ,sqe_low_water_mark_(low_water_mark)
    ,calling_pending_functors_(false)
    ,wakeup_fd_(createEventFd())
    ,input_chunk_manager_(nullptr)
    ,thread_id_(CurrentThread::tid())
{
    LOG_DEBUG("IoUringLoop created %p in thread %d", this, this->thread_id_);
    //one loop per thread,如果t_loopInThisThread不为空，说明当前线程已有一个实例
    if(t_loopInThisThread)
    {
        LOG_FATAL("another eventLoop %p already exists in this thread %d",t_loopInThisThread,this->thread_id_)
    }
    t_loopInThisThread=this;

    //先初始化io_uring ,再初始化内存池
    io_uring_queue_init(ring_size,ring_,0);
    input_chunk_manager_ = std::make_unique<ChunkPoolManagerInput>(*this);
    

}

IoUringLoop::~IoUringLoop()
{
    ::close(this->wakeup_fd_);
    io_uring_queue_exit(ring_);
    delete ring_;
}

void IoUringLoop::loop()
{
    if(quit_) return;
    //执行一次on_completion 把eventfd加入到io_uring中
    on_completion();

    this->looping_=true;
    this->quit_=false;

    LOG_INFO("io_uring_loop %p start looping", this);

    while(!quit_)
    {
        io_uring_submit(ring_);
        //等待cqe返回
        int count = io_uring_peek_batch_cqe(ring_,cqes_.data(),cqes_.size());
        if(count == 0)
        {
            __kernel_timespec ts{time_out_.tv_sec, time_out_.tv_nsec};
            count = io_uring_wait_cqe_timeout(ring_,cqes_.data(),&ts);
            //处理错误
            if(count<0)
            {
                if(-count != ETIME && count != -EINTR)
                {
                    LOG_ERROR("%p io_uring_wait_cqe_timeout error: %s",this,strerror(-count));
                }
                else if(-count == ETIME)
                {
                    LOG_DEBUG("%s time out ! thread: %d",__FUNCTION__,CurrentThread::tid())
                }
            }

            count = io_uring_peek_batch_cqe(ring_,cqes_.data(),cqes_.size());
            this->pollReturnTime_ = Timestamp::now();

        }
        LOG_DEBUG("%d events happend",count)

        //处理cqe中的返回数据
        for(int i=0;i<count;++i)
        {
            //这里可以再做一层封装，比如调用一个函数返回IoContext数组，这里针对io_uring进行特化了
            //但是考虑到上层的connection也要针对io_uring 单独设计，所以这样写感觉还可以
            auto&cqe=cqes_[i];
            //如果user_data为0，则说明这是一个取消任务等不需要任何cqe功能的任务,没有上下文
            if(cqe->user_data==0) continue;

            IoContext*context = reinterpret_cast<IoContext*>(cqe->user_data);
            context->flags_ = cqe->flags;
            context->res_ = cqe->res;

            context->on_completion();
        }

        //推进cq
        if(count!=0) io_uring_cq_advance(ring_,count);


        //执行submit等待队列中的请求
        doingSubmitWaitingTask();
        //执行其它loop追加到这个loop的任务
        this->doingPendingFunctors();
    }

    looping_=false;
    LOG_INFO("EventLoop %p stop looping", this);
}

void IoUringLoop::quit()
{
    quit_=true;
    if(!isInLoopThread())
    {
        wakeUp();
    }
}

void IoUringLoop::runInLoop(Functor cb)
{
    //判断loop对象是否是在当前线程中，如果是，直接执行，否则将cb加入队列，然后唤醒对应的loop执行函数
    if(isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(std::move(cb));
    }
}

void IoUringLoop::queueInLoop(Functor cb)
{
    {
        std::lock_guard<std::mutex>lock(mtx_);
        this->pendingFunctors_.emplace_back(std::move(cb));
    }

    /* 如果是loop对象是在当前线程中，则一定在执行一次loop之后的doingPendingFunctors操作，
       从而calling_pending_functors_一定为true，此时又向队列中加入函数，则下次循环时也需要唤醒
       如果loop对象不在在当前线程中，则直接唤醒
    */
    if(!isInLoopThread()||calling_pending_functors_)
    {
        this->wakeUp();
    }
}

void IoUringLoop::wakeUp()
{
    uint64_t data=1;
    //成功返回写入的字节数，失败返回-1，并设置errno
    ssize_t n=write(this->wakeup_fd_,&data,sizeof(data));
    if(n!=sizeof(data))
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n",n)
    }
}

void IoUringLoop::submitReadMultishut(IoContext* ctx)
{
    if(remainedSqe()<sqe_low_water_mark_)
    {
        waiting_submit_queue_
            .emplace(WaitEntry{ctx,SubmitType::ReadMultishut});
    }
    else
    {
        _submitReadMultishut(ctx);
    }
}

void IoUringLoop::submitWriteMsg(IoContext* ctx)
{
    if(remainedSqe()<sqe_low_water_mark_)
    {
        waiting_submit_queue_
            .emplace(WaitEntry{ctx,SubmitType::WriteMsg});
    }
    else
    {
        _submitWriteMsg(ctx);
    }
}

void IoUringLoop::submitAcceptMultishut(IoContext* ctx)
{
    if(remainedSqe()<sqe_low_water_mark_)
    {
        waiting_submit_queue_
            .emplace(WaitEntry{ctx,SubmitType::AcceptMultishut});
    }
    else
    {
        _submitAcceptMultishut(ctx);
    }
}

void IoUringLoop::submitCancel(IoContext *ctx)
{
    auto sqe = getIoUringSqe(true);

    //这里如果sqe获取失败，直接设置错误码，然后调用on_completion 函数处理错误关闭连接
    assert(sqe&&"sqe should not be nullptr");

    io_uring_prep_cancel(sqe,ctx,0);
    io_uring_sqe_set_data(sqe,0);   //设置data字段为0，表示这个sqe没有上下文

    //这个请求时紧急请求，立即向内核提交一次
    io_uring_submit(ring_);
}

void IoUringLoop::doingPendingFunctors()
{
    //先设置正在执行追加任务的标志为true
    this->calling_pending_functors_=true;
    std::vector<Functor>tasks;
    {   
        /* 用一个临时的容器将队列中的函数转移，避免了在临界区执行过多的时间，从而减慢了运行的效率
            也防止了pendingFunctors_中有追加任务的操作导致重复上锁而导致死锁的问题
         */
        std::lock_guard<std::mutex>lock(this->mtx_);
        tasks.swap(this->pendingFunctors_);
    }
    for(const auto&task:tasks)
    {
        task();
    }
    this->calling_pending_functors_=false;
}

io_uring_sqe *IoUringLoop::getIoUringSqe(bool force_submit)
{
    auto* sqe = io_uring_get_sqe(ring_);
    
    if (!sqe && force_submit) {
        // 强制提交现有请求，再获取
        io_uring_submit(ring_);
        sqe = io_uring_get_sqe(ring_);
        
        if (!sqe) {
            LOG_FATAL("Failed to get SQE even after submit");
        }
    }
    
    return sqe;
}

void IoUringLoop::on_completion()
{
    LOG_DEBUG("%p eventfd: %lu",this,*eventfd_data_addr_);
    
    //eventfd仅用于唤醒操作，在这里再次提交就可以了
    io_uring_sqe* sqe = getIoUringSqe(true);
    
    assert(sqe&&"the sqe should not be nullptr");
    if(!sqe)
    {
        LOG_FATAL("Failed to resubmit eventfd read request. "
                  "This indicates io_uring ring is broken or system resources exhausted. "
                  "Loop %p cannot continue.", this);
    }
    io_uring_prep_read(sqe,wakeup_fd_,eventfd_data_addr_.get(),sizeof(uint64_t),0);
    io_uring_sqe_set_data(sqe,this);
}
