#pragma once

#include<functional>
#include<string>
#include<atomic>
#include<vector>
#include<memory>


#include "noncopyable.h"
#include "IoUringLoop.h"


class IoUringLoopThread;
class IoUringLoopThreadPool: noncopyable
{
public:
    using ThreadInitCallback=std::function<void(IoUringLoop*)>;
private:
    IoUringLoop* base_loop_;//用于分发连接的主loop
    std::string name_;//线程池的名字
    std::atomic_bool started_;//线程池启动的标志
    int next_;//当有连接到达时需要分发给不同的子loop，这个变量表示了下一个要分发连接的loop的下标
    int num_threads_;//线程池中的线程数量

    std::vector<std::unique_ptr<IoUringLoopThread>>threads_;//用于处理连接io请求的io线程
    std::vector<IoUringLoop*>loops_;//io线程中运行的eventloop对象，调用eventloop的函数用于跨线程通信

    IoUringLoopParams loop_params_;

public:
    IoUringLoopThreadPool(IoUringLoop*base_loop,std::string name,IoUringLoopParams loop_params);
    ~IoUringLoopThreadPool();

    void setNumThreads(int num_threads);
    void start(const ThreadInitCallback& cb);
    bool isStart(){return started_.load();}

    //在多线程模式中,baseloop 会默认以轮询的方式将连接分发给subloop中
    IoUringLoop* getNextLoop();
    std::vector<IoUringLoop*>getAllLoops();

    std::string name()const{return name_;}
};


