#include "MonotonicTimestamp.h"
#include <time.h>
#include <chrono>

MonotonicTimestamp::MonotonicTimestamp()
    :microSecondsSinceEpoch_(0)
{
}

MonotonicTimestamp::MonotonicTimestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
{
}

MonotonicTimestamp::MonotonicTimestamp(const MonotonicTimestamp& ts)
    :microSecondsSinceEpoch_(ts.microSecondsSinceEpoch_)
{
}


MonotonicTimestamp& MonotonicTimestamp::operator=(const int64_t microSecondsSinceEpoch)
{
    this->microSecondsSinceEpoch_=microSecondsSinceEpoch;
    return *this;
}

MonotonicTimestamp& MonotonicTimestamp::operator=(const MonotonicTimestamp& ts)
{
    this->microSecondsSinceEpoch_=ts.microSecondsSinceEpoch_;
    return *this;
}

MonotonicTimestamp MonotonicTimestamp::now()
{
    auto now =std::chrono::steady_clock::now();
    int64_t us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    return MonotonicTimestamp(us);
}


