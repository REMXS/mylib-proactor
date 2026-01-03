
#include<cassert>

#include"IoUringLoopThreadPool.h"
#include"IoUringLoopThread.h"
#include"IoUringLoop.h"


IoUringLoopThreadPool::IoUringLoopThreadPool(IoUringLoop*base_loop,std::string name,IoUringLoopParams loop_params)
    :base_loop_(base_loop)
    ,name_(name)
    ,started_(false)
    ,next_(0)
    ,num_threads_(0)
    ,loop_params_(loop_params)
{
}

IoUringLoopThreadPool::~IoUringLoopThreadPool()
{
}


void IoUringLoopThreadPool::setNumThreads(int num_threads)
{
    assert(!started_&&"the thread pool has already started,don't fix the num_threads!");
    num_threads_=num_threads;
}


//开启所有的subloop，如果只有baseloop，则只执行回调函数
void IoUringLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_=true;
    for(int i=0;i<num_threads_;++i)
    {
        std::string buf=name_+std::to_string(i);
        
        //TODO 改变参数传递
        threads_.emplace_back(std::make_unique<IoUringLoopThread>(cb,loop_params_,buf));
        loops_.emplace_back(threads_.back()->startLoop());
    }
    //如果不使用多线程，则base_loop_ 也充当subloop，执行初始化函数
    if(num_threads_==0&&cb)
    {
        cb(base_loop_);
    }
}

IoUringLoop* IoUringLoopThreadPool::getNextLoop()
{

    IoUringLoop* loop=base_loop_;

    //如果不使用多线程就一直返回 baseloop
    if(!loops_.empty())
    {
        loop=loops_[next_];
        next_=(next_+1)%num_threads_;
    }

    return loop;
}


std::vector<IoUringLoop*>IoUringLoopThreadPool::getAllLoops()
{
    if(!loops_.empty())
    {
        return loops_;
    }
    else
    {
        return std::vector<IoUringLoop*>(1,base_loop_);
    }
}