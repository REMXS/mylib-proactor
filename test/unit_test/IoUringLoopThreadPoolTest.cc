#include "test_helper.h"
#include "IoUringLoopThreadPool.h"
#include "CurrentThread.h" 
#include "Logger.h"
#include <memory>
#include <vector>
#include <set>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <chrono>

// 1. 创建测试固件
class IoUringLoopThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试用例开始前，都会创建一个新的 baseLoop 和 pool
        std::thread t([&](){baseLoop_ = std::make_unique<IoUringLoop>(param);});

        t.join();
        pool_ = std::make_unique<IoUringLoopThreadPool>(baseLoop_.get(), "TestPool",param);
    }

    void TearDown() override {
        // unique_ptr 会自动处理对象的销毁，无需手动操作
    }

    std::unique_ptr<IoUringLoop> baseLoop_;
    std::unique_ptr<IoUringLoopThreadPool> pool_;
    IoUringLoopParams param{512,32,32};
};

// 2. 测试单线程模式 (numThreads = 0)
TEST_F(IoUringLoopThreadPoolTest, SingleThreadMode) {
    // 设置线程数为0
    pool_->setNumThreads(0);
    auto fu=std::function<void(IoUringLoop*)>();
    pool_->start(fu);

    // 验证 getNextLoop() 是否总是返回 baseLoop
    ASSERT_EQ(pool_->getNextLoop(), baseLoop_.get());
    ASSERT_EQ(pool_->getNextLoop(), baseLoop_.get());

    // 验证提交到 loop 的任务可以被执行
    bool executed = false;
    std::mutex mtx;
    std::condition_variable cv;

    baseLoop_->runInLoop([&]() {
        std::lock_guard<std::mutex> lock(mtx);
        executed = true;
        cv.notify_one();
    });

    // 启动 baseLoop，它会执行上面的任务然后退出
    std::thread t1([&](){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        baseLoop_->quit();
    });

    baseLoop_->loop();
    //等待任务执行完成的信号
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]{ return executed; });
    
    ASSERT_TRUE(executed);
    
    t1.join();
}

// 3. 测试多线程模式 (numThreads > 0)
TEST_F(IoUringLoopThreadPoolTest, MultiThreadMode) {
    const int kThreadNum = 3;
    pool_->setNumThreads(kThreadNum);
    auto fu=std::function<void(IoUringLoop*)>();
    pool_->start(fu);

    // 验证 getNextLoop() 是否按轮询顺序返回不同的 loop
    IoUringLoop* loop1 = pool_->getNextLoop();
    IoUringLoop* loop2 = pool_->getNextLoop();
    IoUringLoop* loop3 = pool_->getNextLoop();
    IoUringLoop* loop4 = pool_->getNextLoop();

    // 验证轮询
    ASSERT_NE(loop1, baseLoop_.get());
    ASSERT_NE(loop2, baseLoop_.get());
    ASSERT_NE(loop3, baseLoop_.get());
    ASSERT_NE(loop1, loop2);
    ASSERT_NE(loop1, loop3);
    ASSERT_NE(loop2, loop3);
    ASSERT_EQ(loop4, loop1); // 轮询一圈后回到第一个

    // 验证任务是否在不同的线程中执行
    std::mutex mtx;
    std::condition_variable cv;
    std::set<pid_t> thread_ids;
    int task_count = 0;

    auto task = [&](IoUringLoop* loop) {
        loop->runInLoop([&]() {
            std::lock_guard<std::mutex> lock(mtx);
            thread_ids.insert(CurrentThread::tid());
            task_count++;
            if (task_count == kThreadNum) {
                cv.notify_one();
            }
        });
    };

    // 向每个 sub-loop 派发一个任务
    task(loop1);
    task(loop2);
    task(loop3);

    // 等待所有任务执行完成
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return task_count == kThreadNum; });
    }

    // 验证任务确实在3个不同的线程中执行了
    ASSERT_EQ(thread_ids.size(), kThreadNum);

    // 为了让测试优雅退出，需要停止所有 loop
    // 注意：在实际的 muduo 中，loop 的退出是由 TcpServer/TcpConnection 的生命周期管理的
    // 在这个单元测试中，我们可以手动退出它们
    loop1->quit();
    loop2->quit();
    loop3->quit();
}

//测试mainloop通过eventfd通知subloop
TEST_F(IoUringLoopThreadPoolTest, MultiThreadMode_IoUringFdNotice)
{
    LOG_INFO("thread id :%d",CurrentThread::tid())
    const int kThreadNum = 3;
    pool_->setNumThreads(kThreadNum);


    std::atomic<int> task_count=0;
    std::condition_variable cv;
    std::mutex mtx;

    /* 
    原本的流程为baseloop监听接收连接请求的fd，poller返回时执行这个fd的读回调函数，
    向subloop中分发请求，就是向subloop的任务队列中加入注册channel的函数，
    这个测试将分发请求的过程放到了执行fd处理函数之后的处理loop本身回调函数的阶段
    */
    auto wakeUpFunc=[&](){
        std::vector<IoUringLoop*>subloops=pool_->getAllLoops();
        assert(subloops.size()==kThreadNum&&"wakeUpFunc:the num of subloop is wrong");

        for(auto subloop:subloops)
        {
            subloop->runInLoop([&](){
                task_count++;
                if(task_count==kThreadNum) cv.notify_one();
            });
        }
    };
    auto closeFunc=[&](){
        std::vector<IoUringLoop*>subloops=pool_->getAllLoops();
        assert(subloops.size()==kThreadNum&&"closeFunc:the num of subloop is wrong");

        //要对变量的声明周期有明确的认识，这是临时变量拷贝
        for(auto subloop:subloops)
        {
            /*
            这里不要使用引用捕获，因为subloop是临时变量，
            因为这是回调函数，不会立即执行，而subloop这是个临时变量，
            很快就会销毁，到时候执行回调函数的时候就会有一个已销毁对象引用，从而触发core dump 
            只要是指针，大部分情况要用值捕获
            */
            subloop->runInLoop([subloop](){
                subloop->quit();
            });
        }
    };

    std::atomic<int> call_back_trigger=0;

    auto init_call_back=std::function<void(IoUringLoop*)>([&](IoUringLoop*el){
        call_back_trigger++;
    });

    //在另一个线程中开启一个任务，向主loop中写入回调函数
    std::thread other_thread([&](){
        baseLoop_->runInLoop(wakeUpFunc);
        baseLoop_->runInLoop(closeFunc);
        baseLoop_->runInLoop([&](){baseLoop_->quit();});
    });

    pool_->start(init_call_back);

    baseLoop_->loop();
    other_thread.join();

    /* 理论上init_call_back(loop初始化执行的回调函数)是要在loop之前执行的，
    所以在task_count为kThreadNum之前call_back_trigger必须为kThreadNum */
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return task_count == kThreadNum; });
    }

    ASSERT_EQ(task_count,kThreadNum);
    ASSERT_EQ(call_back_trigger,kThreadNum);
}