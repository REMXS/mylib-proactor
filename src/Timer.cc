#include "Timer.h"

void Timer::restart(MonotonicTimestamp now)
{
    if(repeat_)
    {
        expiration_ = addTime(now,interval_);
    }
    else
    {
        expiration_ = MonotonicTimestamp::invaild();
    }
}
