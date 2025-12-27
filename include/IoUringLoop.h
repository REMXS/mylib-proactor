#pragma once

#include <liburing.h>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class IoUringLoop: noncopyable
{
private:
    using Functor=std::function<void()>;
    io_uring* ring_;
    std::vector<io_uring_cqe*>cqes_;
    std::atomic_bool looping_;
    std::atomic_bool quit_;
    //拥有此eventloop的线程id，每一个eventloop对应一个线程，一个线程只允许存在一个eventloop
    const pid_t thread_id_;


    //每次循环调用poller时的时间点
    Timestamp pollReturnTime_; 

    //用于跨线程唤醒操作
    int wakeup_fd_;
    //接收wakeupfd_数据的变量
    std::unique_ptr<int>wakeup_fd_data_;


    //等待被执行的任务队列,访问此变量时需要加锁
    std::vector<Functor>pendingFunctors_;
    //此loop是否在执行任务队列中任务的标志
    std::atomic_bool calling_pending_functors_;
    //可能有其它线程在任务队列中追加任务，所以要加锁
    std::mutex mtx_;
public:
    IoUringLoop(size_t ring_size,size_t cqes_size);
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

    void submit_read();
    void submit_write();
    void submit_accept();
};

