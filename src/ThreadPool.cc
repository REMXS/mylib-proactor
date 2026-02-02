#include "ThreadPool.h"

void ThreadPool::enqueue(std::coroutine_handle<> h)
{
    {
        std::lock_guard<std::mutex>lock(mtx_);
        tasks_.emplace(h);
    }
    //唤醒一个线程执行任务
    cv_.notify_one();
}

ThreadPool::ThreadPool(size_t thread_num, size_t queue_size)
    : max_size_(queue_size), started_(true)
{
    for(int i=0;i<thread_num;++i)
    {
        threads_.emplace_back(std::thread([this](){
            std::unique_lock<std::mutex>lock(mtx_);
            cv_.wait(lock,[&](){
                return !started_.load()||!tasks_.empty();
            });

            //如果线程池停止并且任务队列中没有任务的话，直接退出
            if(!started_.load()&&tasks_.empty()) return;
            std::coroutine_handle<>handel = tasks_.front();
            tasks_.pop();
            lock.unlock();

            //执行任务
            handel.resume();
        }));
    }
}

ThreadPool::~ThreadPool()
{
    stop();
    cv_.notify_all();
    for(auto&t:threads_)
    {
        if(t.joinable()) t.join();
    }
}

void ThreadPool::stop()
{
    started_.store(false);
}

SwitchThreadAwaiter ThreadPool::switchThread()
{
    return SwitchThreadAwaiter(*this);
}

bool ThreadPool::isFull()
{
    return tasks_.size()>=max_size_;
}

SwitchThreadAwaiter::SwitchThreadAwaiter(ThreadPool &pool)
    :pool_(pool)
    ,success_(true)
{}

bool SwitchThreadAwaiter::await_ready()
{
    //如果线程池任务已满，直接返回false不阻塞
    if(pool_.isFull()){
        success_ = false;
        return true;
    }
    return false;
}

void SwitchThreadAwaiter::await_suspend(std::coroutine_handle<> h)
{
    pool_.enqueue(h);
}

bool SwitchThreadAwaiter::await_resume()
{
    return success_;
}
