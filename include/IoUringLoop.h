#pragma once

#include <liburing.h>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>
#include <queue>

#include "noncopyable.h"
#include "Timestamp.h"
#include "MonotonicTimestamp.h"
#include "CurrentThread.h"
#include "IoContext.h"
#include "TimerId.h"
#include "Callbacks.h"

class Acceptor;
class ChunkPoolManagerInput;
class TimerQueue;

struct IoUringLoopParams
{
    size_t ring_size_;
    size_t cqes_size_;
    size_t low_water_mark_;
};

class IoUringLoop: noncopyable , IoContext
{
private:
    friend ChunkPoolManagerInput;

    using Functor=std::function<void()>;
    io_uring* ring_;
    std::vector<io_uring_cqe*>cqes_;
    std::atomic_bool looping_;
    std::atomic_bool quit_;
    //拥有此eventloop的线程id，每一个eventloop对应一个线程，一个线程只允许存在一个eventloop
    const pid_t thread_id_;
    timespec time_out_;
    size_t sqe_low_water_mark_;     //sqe的低水位，如果低于这个值就触发背压逻辑

    //buffer ring的内存池
    std::unique_ptr<ChunkPoolManagerInput>input_chunk_manager_;


    //每次循环调用poller时的时间点
    Timestamp pollReturnTime_; 


    //等待被执行的任务队列,访问此变量时需要加锁
    std::vector<Functor>pendingFunctors_;
    //此loop是否在执行任务队列中任务的标志
    std::atomic_bool calling_pending_functors_;
    //可能有其它线程在任务队列中追加任务，所以要加锁
    std::mutex mtx_;


    //用于跨线程唤醒操作
    int wakeup_fd_;
    //接收wakeupfd_数据的context
    std::unique_ptr<uint64_t>eventfd_data_addr_;
    void on_completion()override;

    //在每次循环结束的时候，执行任务队列中的额外任务，任务多为上层回调
    void doingPendingFunctors();

    io_uring_sqe* getIoUringSqe(bool force_submit);

    //用于sqe耗尽时的背压操作相关的资源
    enum class SubmitType
    {
        ReadMultishut,
        WriteMsg,
        AcceptMultishut
    };
    using WaitEntry = std::pair<IoContext*,SubmitType>;
    std::queue<WaitEntry>waiting_submit_queue_;
    void doingSubmitWaitingTask();

    //定时器相关
    std::unique_ptr<TimerQueue>timer_queue_;
    //获取最近的超时时间
    __kernel_timespec getTimeOutPeriod();

    //内部实际执行的提交操作
    void _submitReadMultishut(IoContext* ctx);
    void _submitWriteMsg(IoContext* ctx);
    void _submitAcceptMultishut(IoContext* ctx);

public:
    IoUringLoop(size_t ring_size,size_t cqes_size,size_t low_water_mark = 0);
    IoUringLoop(const IoUringLoopParams& params);
    ~IoUringLoop();

    //开启事件循环
    void loop();
    //停止事件循环
    void quit();

    //获取最近一次poller返回的时间
    Timestamp getPollReturnTime(){return this->pollReturnTime_;}

    //在指定loop中执行任务
    void runInLoop(Functor cb);
    //将任务追加到任务队列当中并唤醒loop执行任务
    void queueInLoop(Functor cb);

    //通过eventfd唤醒loop执行队列中的任务
    void wakeUp();

    //判断此eventloop知否在当前线程中
    bool isInLoopThread()const {return this->thread_id_==CurrentThread::tid();}

    void submitReadMultishut(IoContext* ctx);
    void submitWriteMsg(IoContext* ctx);
    void submitAcceptMultishut(IoContext* ctx);
    void submitCancel(IoContext* ctx);

    uint32_t remainedSqe()const {return io_uring_sq_space_left(ring_);}

    ChunkPoolManagerInput& getInputPool() {return *input_chunk_manager_;}

    //定时器相关
    //在固定时间执行定时任务,注意when应当是相对时间
    TimerId runAt(MonotonicTimestamp when,TimerCallback cb);
    //在当前时间之后执行定时任务
    TimerId runAfter(double delay,TimerCallback cb);
    //执行重复触发的定时任务
    TimerId runEveny(double interval,TimerCallback cb);
    //取消定时任务
    void cancel(TimerId timer_id);
};

