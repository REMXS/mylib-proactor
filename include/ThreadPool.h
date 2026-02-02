#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <coroutine>
#include <atomic>
#include <vector>

#include "noncopyable.h"

class SwitchThreadAwaiter;
class ThreadPool :noncopyable
{
    friend SwitchThreadAwaiter;
private:
    std::mutex mtx_;
    std::condition_variable cv_;

    //存放协程任务的队列
    std::queue<std::coroutine_handle<>>tasks_;
    std::atomic_bool started_;
    size_t max_size_;
    std::vector<std::thread>threads_;

    void enqueue(std::coroutine_handle<>h);
public:
    ThreadPool(size_t thread_num,size_t queue_size);
    ~ThreadPool();

    //禁止移动
    ThreadPool(ThreadPool&&)=delete;
    ThreadPool& operator = (ThreadPool&&)=delete;

    void stop();
    bool isStarted()const {return started_.load();}

    SwitchThreadAwaiter switchThread();

    bool isFull();
};

class SwitchThreadAwaiter
{
private:
    ThreadPool& pool_;
    bool success_;
public:
    SwitchThreadAwaiter(ThreadPool& pool);
    ~SwitchThreadAwaiter()=default;

    bool await_ready();

    void await_suspend(std::coroutine_handle<>h);

    bool await_resume();
};

