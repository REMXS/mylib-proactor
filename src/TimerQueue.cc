#include <cassert>
#include <sys/timerfd.h>
#include <cstring>
#include <chrono>

#include "TimerQueue.h"
#include "IoUringLoop.h"
#include "TimerId.h"
#include "Timer.h"
#include "Logger.h"

int createTimerFd()
{
    int timer_fd = ::timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);
    if(timer_fd==-1)
    {
        LOG_FATAL("timerfd_create failed! errno:%s",strerror(errno));
    }
    return timer_fd;
}

struct timespec howMuchTimeFromNow(MonotonicTimestamp when,MonotonicTimestamp now = MonotonicTimestamp::now())
{
    std::chrono::microseconds microseconds(when.microSecondsSinceEpoch() -now.microSecondsSinceEpoch());
    if(microseconds.count()<100)
    {
        microseconds=std::chrono::microseconds(100);
    }
    struct timespec ts;
    ts.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(microseconds).count();
    ts.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(microseconds%std::chrono::seconds(1)).count();
    return ts;
}

TimerId TimerQueue::addTimer(TimerCallback cb, MonotonicTimestamp when, double interval)
{
    //创建timer对象
    Timer* raw_timer_ptr = new Timer(std::move(cb),when,interval);

    //！此处可能出现概率极低的内存泄露风险
    loop_.runInLoop([this,raw_timer_ptr](){this->addTimerInLoop(raw_timer_ptr);});

    return TimerId(raw_timer_ptr,raw_timer_ptr->sequence());
}

void TimerQueue::cancelTimer(TimerId timer_id)
{
    loop_.runInLoop([this,timer_id = std::move(timer_id)](){this->cancelTimerInLoop(std::move(timer_id));});
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
    assert(loop_.isInLoopThread());
    std::unique_ptr<Timer>u_timer(timer);

    insert(std::move(u_timer));
}

void TimerQueue::cancelTimerInLoop(TimerId timer_id)
{
    assert(loop_.isInLoopThread());
    assert(timer_list_.size()==active_timer_set_.size());

    //删除节点
    ActiveTimer del_timer(timer_id.timer_,timer_id.sequence_);
    if(is_running_callback_)
    {
        canceling_timer_set_.emplace(del_timer);
    }
    else
    {
        erase(del_timer);
    }

    assert(timer_list_.size()==active_timer_set_.size());
}

void TimerQueue::handleRead(MonotonicTimestamp now)
{
    LOG_DEBUG("timer queue size : %ld",timer_list_.size());
    //优化：先判断是否有超时的定时器，如果没有，直接返回
    if(timer_list_.empty()||now<timer_list_.begin()->first.first) return;

    assert(timer_list_.size()==active_timer_set_.size());

    //获取所有过期的timer
    std::vector<Entry>expired_timers=getExpiredTimers(now);

    is_running_callback_ = true;
    //执行定时器的回调函数
    for(auto&entry:expired_timers)
    {
        //注意重入导致的迭代器失效问题
        timer_list_[entry]->run();
    }
    is_running_callback_ = false;

    //重置过期的timer
    reset(expired_timers,now);

    assert(timer_list_.size()==active_timer_set_.size());
}

std::vector<TimerQueue::Entry> TimerQueue::getExpiredTimers(MonotonicTimestamp now)
{
    assert(timer_list_.size()==active_timer_set_.size());
    std::vector<Entry>ret;
    Entry edge_entry(now,LLONG_MAX); 
    //获取第一个大于等于当前时间的定时器
    TimerList::iterator end = timer_list_.upper_bound(edge_entry);
    assert(end == timer_list_.end()|| end->second->expiration()>now);
    
    for(auto start = timer_list_.begin();start!=end;++start)
    {
        ret.emplace_back(start->first);
    }

    return ret;
}

void TimerQueue::reset(const std::vector<Entry> expired_timers, MonotonicTimestamp now)
{
    for(auto&entry:expired_timers)
    {
        ActiveTimer temp_ac(timer_list_[entry].get(),entry.second);
        //如果是重复定时器，而且没有出现在canceling_timer_set_中，则重新定时
        if(timer_list_[entry]->repeat()&&canceling_timer_set_.find(temp_ac)==canceling_timer_set_.end())
        {
            std::unique_ptr<Timer>temp_ptr = std::move(timer_list_[entry]);
            temp_ptr->restart(now);
            //擦除旧索引，构建新索引
            timer_list_.erase(entry);
            Entry new_entry(temp_ptr->expiration(),entry.second);
            timer_list_[new_entry]=std::move(temp_ptr);
        }
        //如果不是，直接删除
        else
        {
            erase(temp_ac);
            canceling_timer_set_.erase(temp_ac);
        }
    }
    //删除canceling_timer_set_中剩余的元素
    for(auto&del_timer:canceling_timer_set_)
    {
        erase(del_timer);
    }
    canceling_timer_set_.clear();
}

bool TimerQueue::insert(std::unique_ptr<Timer> timer)
{
    assert(loop_.isInLoopThread());
    assert(timer_list_.size()==active_timer_set_.size());

    bool is_earily_changed = false;

    Entry new_entry(timer->expiration(),timer->sequence());
    ActiveTimer new_timer(timer.get(),new_entry.second);

    //判断是否是队列中的第一个元素
    if(timer_list_.empty()||new_entry.first<timer_list_.begin()->first.first)
    {
        is_earily_changed = true;
    }

    timer_list_[new_entry]=std::move(timer);
    auto[_,success] = active_timer_set_.emplace(new_timer);
    assert(success);

    assert(timer_list_.size()==active_timer_set_.size());
    return is_earily_changed;
}

void TimerQueue::erase(ActiveTimer del_timer)
{
    auto it = active_timer_set_.find(del_timer);
    if(it !=active_timer_set_.end())
    {
        Entry del_entry(it->first->expiration(),it->first->sequence());

        active_timer_set_.erase(del_timer);
        timer_list_.erase(del_entry);
    }
}

timespec TimerQueue::getRecentExpireTime(MonotonicTimestamp now)
{
    if(timer_list_.empty()) return {LONG_MAX,0};
    return howMuchTimeFromNow(timer_list_.begin()->first.first,now);
}

TimerQueue::TimerQueue(IoUringLoop &loop)
    :loop_(loop)
    ,is_running_callback_(false)
{
}

TimerQueue::~TimerQueue()
{
}
