#pragma once
#include <iostream>

class Timer;

//给用户使用的外部句柄
class TimerId
{
private:
    Timer* timer_;
    int64_t sequence_;
public:
    friend class TimerQueue;

    TimerId()
        :timer_(nullptr)
        ,sequence_(0)
    {}

    TimerId(Timer* timer,int64_t seq)
        :timer_(timer)
        ,sequence_(seq)
    {}

    ~TimerId()=default;
};

