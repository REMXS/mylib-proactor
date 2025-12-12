#pragma once

#include <iostream>
#include <string>
#include <chrono>

class MonotonicTimestamp
{
public:
    MonotonicTimestamp();
    explicit MonotonicTimestamp(int64_t microSecondsSinceEpoch);
    MonotonicTimestamp(const MonotonicTimestamp& ts);

    MonotonicTimestamp& operator=(int64_t microSecondsSinceEpoch);
    MonotonicTimestamp& operator=(const MonotonicTimestamp& ts);
    
    bool operator<(const MonotonicTimestamp& ts) const {return microSecondsSinceEpoch_<ts.microSecondsSinceEpoch_;}
    bool operator==(const  MonotonicTimestamp& ts) const {return microSecondsSinceEpoch_==ts.microSecondsSinceEpoch_;}
    bool operator!=(const  MonotonicTimestamp& ts) const {return microSecondsSinceEpoch_!=ts.microSecondsSinceEpoch_;}
    bool operator>(const MonotonicTimestamp& ts) const {return microSecondsSinceEpoch_>ts.microSecondsSinceEpoch_;}


    //获取当前时间
    static MonotonicTimestamp now();
    //返回空时间
    static MonotonicTimestamp invaild(){return MonotonicTimestamp();}

    inline int64_t microSecondsSinceEpoch()const {return microSecondsSinceEpoch_;}

    bool vaild()const {return microSecondsSinceEpoch_>0;}

private:
    int64_t microSecondsSinceEpoch_ ;
};


inline MonotonicTimestamp addTime(MonotonicTimestamp m_timestamp,double seconds)
{
    std::chrono::duration<double>seconds_duration(seconds);
    //将秒转换为微秒
    auto microseconds_duration = std::chrono::duration_cast<std::chrono::microseconds>(seconds_duration);
    //返回时间的加和
    return MonotonicTimestamp(m_timestamp.microSecondsSinceEpoch()+microseconds_duration.count());
}