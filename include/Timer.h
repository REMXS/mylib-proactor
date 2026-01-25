#pragma once

#include <atomic>

#include "MonotonicTimestamp.h"
#include "Callbacks.h"
#include "noncopyable.h"

class Timer: noncopyable
{
private:
    const TimerCallback callback_; //时间到触发的回调函数
    MonotonicTimestamp expiration_;      //过期时间
    const double interval_;     //定时器触发的间隔，如果大于0说明是重复触发的任务
    bool repeat_;               //标记是否为重复任务，配合interval_进行使用
    int64_t sequence_;          //定时器的唯一编号，用于区分不同的定时器实例
    
    inline static std::atomic_int64_t s_global_sequence_{0};    //记录创建定时器的下一个编号
public:
    Timer(TimerCallback callback,MonotonicTimestamp when,double interval)
        :callback_(std::move(callback))
        ,expiration_(when)
        ,interval_(interval)
        ,repeat_(interval_>0.0)
        ,sequence_(s_global_sequence_.fetch_add(1))
    {}

    ~Timer()=default;

    MonotonicTimestamp expiration()const {return expiration_;} 
    bool repeat()const {return repeat_;}    
    void run()const {if(callback_) callback_();}
    int64_t sequence()const {return sequence_;}

    void restart(MonotonicTimestamp now);

    static int64_t numCreated() {return s_global_sequence_.load();}     //返回当前创建的定时器的下一个编号

};


