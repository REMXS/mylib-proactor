#pragma once

#include<memory>
#include<functional>
#include<string>
#include<atomic>
#include<mutex>
#include<condition_variable>

#include"Thread.h"
#include"noncopyable.h"
#include"IoUringLoop.h"

class IoUringLoop;

class IoUringLoopThread:noncopyable
{
public:
    using ThreadInitCallback=std::function<void(IoUringLoop*)>;
private:
    std::shared_ptr<Thread>thread_;
    IoUringLoop* loop_;
    std::atomic_bool exiting_;
    ThreadInitCallback call_back_;//执行的初始化回调函数，用于在loop中做一些初始操作

    //因为在子线程中需要创建loop，而主线程又需要得到这个loop，从而需要loop_这个中间量传递，因此要加锁
    std::mutex mtx_;
    std::condition_variable cv_;

    std::string name_;

    IoUringLoopParams loop_params_;     //构造loop所需要的参数的集合

    void ThreadFunc();//要在子线程中执行的函数

public:
    IoUringLoop* startLoop();//因为主eventloop在分发连接的时候需要调用其它loop中的wakeup函数，所以主loop需要知道其它loop对象
    


    IoUringLoopThread(const ThreadInitCallback&cb,IoUringLoopParams loop_params,const std::string&name="");
    ~IoUringLoopThread();
};


