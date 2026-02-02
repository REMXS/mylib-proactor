#include "test_helper.h"

#include "Task.hpp"
#include "ThreadPool.h"
#include "CurrentThread.h"

#include <future>
#include <chrono>

Task<size_t>ThreadPoolTestFunc(std::promise<bool>pm,ThreadPool&pool)
{
    int number = 1;

    //切换线程执行
    bool is = co_await pool.switchThread();
    number++;
    pm.set_value(is);
    co_yield CurrentThread::tid();

    co_return (size_t)number;
}

TEST(ThreadPoolTest,baseTest)
{
    
    ThreadPool pool(2,3);
    std::promise<bool>pm;
    std::future<bool>fu = pm.get_future();
    //创建任务并执行
    auto task = ThreadPoolTestFunc(std::move(pm),pool);
    task.resume();

    EXPECT_TRUE(fu.get());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_NE(task.currentValue(),CurrentThread::tid());
    
    task.resume();
    EXPECT_EQ(task.returnValue(),2);
    pool.stop();
}