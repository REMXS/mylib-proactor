#include "test_helper.h"
#include <memory>

#include "TimerQueue.h"
#include "IoUringLoop.h"
#include "TimerId.h"


class TimerQueueTest: public ::testing::Test
{
protected:
    std::unique_ptr<IoUringLoop> loop;

    void SetUp() override
    {
        loop = std::make_unique<IoUringLoop>(1024,32);
    }

    void TearDown() override
    {
        loop.reset();
    }
};

//测试基本定时器的添加和触发
TEST_F(TimerQueueTest, basic_timer)
{
    bool is_called=false;
    loop->runAt(addTime(MonotonicTimestamp::now(),0.1),[&](){
        is_called=true;
        loop->quit();
    });

    loop->loop();
    EXPECT_TRUE(is_called);
}

//测试重复定时器
TEST_F(TimerQueueTest, repeating_timer)
{
    int call_count=0;
    loop->runEveny(0.1,[&](){
        call_count++;
        if(call_count==3)
        {
            loop->quit();
        }
    });

    loop->loop();
    EXPECT_EQ(call_count,3);
}

//测试取消定时器
TEST_F(TimerQueueTest, cancel_timer)
{
    bool should_not_be_called=false;
    bool should_be_called=false;

    TimerId timer_id = loop->runAt(addTime(MonotonicTimestamp::now(), 0.1),[&](){
        should_not_be_called=true;
    });

    loop->cancel(timer_id);

    loop->runAt(addTime(MonotonicTimestamp::now(), 0.2),[&](){
        should_be_called=true;
        loop->quit();
    });

    loop->loop();
    EXPECT_FALSE(should_not_be_called);
    EXPECT_TRUE(should_be_called);
}

//测试取消正在执行的定时器
TEST_F(TimerQueueTest, cancel_during_callback)
{
    //添加一个重复定时器，在其回调中取消自身
    int cnt=0;
    TimerId timer_id = loop->runEveny(0.1,[&](){
        cnt++;
        if(cnt==2)
        {
            loop->cancel(timer_id);
            loop->runAt(addTime(MonotonicTimestamp::now(), 0.1),[&](){
                loop->quit();
            });
        }
    });

    loop->loop();
    EXPECT_EQ(cnt,2);
}

//测试相同时间点添加多个定时器
TEST_F(TimerQueueTest, multiple_timers_same_time)
{
    int cnt=0;
    auto timer_task=[&](){
        cnt++;
    };

    MonotonicTimestamp mt = addTime(MonotonicTimestamp::now(), 0.1);
    TimerId timer_id1 = loop->runAt(mt,timer_task);
    loop->runAt(mt,timer_task);
    loop->runAt(mt,timer_task);
    loop->runAt(mt,timer_task);
    loop->cancel(timer_id1);

    loop->runAt(addTime(MonotonicTimestamp::now(), 0.2),[&](){
        loop->quit();
    });

    loop->loop();
    EXPECT_EQ(cnt,3);
}




