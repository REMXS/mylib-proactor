#include "test_helper.h"

#include "Timer.h"
TEST(TimerTest, BasicTimer_test){
    bool called = false;
    TimerCallback cb = [&called]() {
        called = true;
    };

    MonotonicTimestamp now = MonotonicTimestamp::now();
    double interval = 0.1; // 100 milliseconds
    Timer timer(cb, addTime(now, interval), interval);

    EXPECT_EQ(timer.repeat(), true);
    EXPECT_EQ(timer.expiration().microSecondsSinceEpoch() > now.microSecondsSinceEpoch(), true);

    // 运行回调函数
    timer.run();
    EXPECT_EQ(called, true);

    MonotonicTimestamp oldExpiration = timer.expiration();
    timer.restart(MonotonicTimestamp::now());
    EXPECT_EQ(timer.expiration().microSecondsSinceEpoch() > oldExpiration.microSecondsSinceEpoch(), true);
}
TEST(TimerTest, OneShotTimer_test){
    bool called = false;
    TimerCallback cb = [&called]() {
        called = true;
    };

    MonotonicTimestamp now = MonotonicTimestamp::now();
    double interval = 0.0; // One-shot timer
    Timer timer(cb, addTime(now, interval), interval);

    EXPECT_EQ(timer.repeat(), false);
    EXPECT_EQ(timer.expiration().microSecondsSinceEpoch() == now.microSecondsSinceEpoch(), true);

    // 运行回调函数
    timer.run();
    EXPECT_EQ(called, true);

    timer.restart(MonotonicTimestamp::now());
    EXPECT_EQ(timer.expiration().microSecondsSinceEpoch(), 0); // Should be invalid timestamp
}