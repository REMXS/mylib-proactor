#pragma once

#include <iostream>
#include <string>
#include <chrono>

class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    Timestamp(const Timestamp& ts);

    Timestamp& operator=(int64_t microSecondsSinceEpoch);
    Timestamp& operator=(const Timestamp& ts);
    
    bool operator<(const Timestamp& ts) const {return microSecondsSinceEpoch_<ts.microSecondsSinceEpoch_;}
    bool operator==(const  Timestamp& ts) const {return microSecondsSinceEpoch_==ts.microSecondsSinceEpoch_;}
    bool operator!=(const  Timestamp& ts) const {return microSecondsSinceEpoch_!=ts.microSecondsSinceEpoch_;}
    bool operator>(const Timestamp& ts) const {return microSecondsSinceEpoch_>ts.microSecondsSinceEpoch_;}


    //获取当前时间
    static Timestamp now();
    //返回空时间
    static Timestamp invaild(){return Timestamp();}

    inline int64_t microSecondsSinceEpoch()const {return microSecondsSinceEpoch_;}

    //获取当前时间的字符串类型
    std::string to_string() const;

    bool vaild()const {return microSecondsSinceEpoch_>0;}

private:
    int64_t microSecondsSinceEpoch_ ;
};


inline Timestamp addTime(Timestamp timestamp,double seconds)
{
    std::chrono::duration<double>seconds_duration(seconds);
    //将秒转换为微秒
    auto microseconds_duration = std::chrono::duration_cast<std::chrono::microseconds>(seconds_duration);
    //返回时间的加和
    return Timestamp(timestamp.microSecondsSinceEpoch()+microseconds_duration.count());
}