#pragma once
#include <memory>
#include <map>
#include <set>
#include <vector>

#include "noncopyable.h"
#include "Callbacks.h"
#include "MonotonicTimestamp.h"
#include "IoContext.h"


class IoUringLoop;
class Timer;
class TimerId;


class TimerQueue: noncopyable
{
private:
    using TimerPtr = std::unique_ptr<Timer>;
    using Entry = std::pair<MonotonicTimestamp,int64_t>;
    using TimerList = std::map<Entry,TimerPtr>;

    using ActiveTimer = std::pair<Timer*,int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;


    IoUringLoop& loop_;

    TimerList timer_list_;
    ActiveTimerSet active_timer_set_;
    ActiveTimerSet canceling_timer_set_;
    bool is_running_callback_;

    void addTimerInLoop(Timer* timer);
    void cancelTimerInLoop(TimerId timer_id);

    //获取过期的定时器
    std::vector<Entry>getExpiredTimers(MonotonicTimestamp now);
    //处理过期的定时器
    void reset(const std::vector<Entry>expired_timers,MonotonicTimestamp now);
    //内部插入timer的具体实现
    bool insert(std::unique_ptr<Timer> timer);
    //内部删除timer的具体实现
    void erase(ActiveTimer del_timer);

public:
    explicit TimerQueue(IoUringLoop& loop);
    ~TimerQueue();

    TimerId addTimer(TimerCallback cb,MonotonicTimestamp when, double interval);
    void cancelTimer(TimerId timer_id);

    //具体执行操作的函数
    void handleRead(MonotonicTimestamp now=MonotonicTimestamp::now());
    
    //获取当前最小的超时时间
    timespec getRecentExpireTime(MonotonicTimestamp now=MonotonicTimestamp::now());
};


