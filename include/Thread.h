#pragma once


#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

#include"noncopyable.h"

class Thread:noncopyable , public std::enable_shared_from_this<Thread>
{
public:
    using ThreadFunc=std::function<void()>;
private:
    std::string name_;//线程的名字
    bool started_;//线程是否开始执行
    bool joined_;//线程是否执行了join
    ThreadFunc func_;//线程中执行的函数

    std::unique_ptr<std::thread>thread_;//具体的线程对象

    static std::atomic_int num_created; //所有线程对象共享，如果线程没有名称，则用这个编号作为名称

    pid_t tid_; //线程的tid

    void setDefaultName();

public:
    void start();
    void join();

    bool isStarted(){return this->started_;}

    pid_t tid(){return this->tid_;}

    const std::string&name()const {return this->name_;}
    
    static int numCreated(){return num_created;}

    explicit Thread(ThreadFunc func,std::string name ="");
    ~Thread();
};

