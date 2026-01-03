#include<future>
#include<assert.h>

#include"Thread.h"
#include"CurrentThread.h"

std::atomic_int Thread::num_created(0);


Thread::Thread(ThreadFunc func,std::string name)
    :name_(name)
    ,started_(false)
    ,joined_(false)
    ,func_(std::move(func))
{
    setDefaultName();
}

Thread::~Thread()
{
    //如果线程启动了而且还没进行join操作，执行detach，符合规范，也多了一层保险
    if(started_&&!joined_)
    {
        thread_->detach();
    }
}

void Thread::setDefaultName()
{
    int num=++num_created;

    //如果没有名字才使用默认名字，如果有就直接返回就可以了
    if(name_.empty())
    {
        char buf[32]={0};
        snprintf(buf,sizeof(buf),"Thread%d",num);
        name_=buf;
    }
}


void Thread::start()
{
    started_=true;
    std::shared_ptr<Thread>self=shared_from_this();

    std::promise<void>promise;
    std::future<void>fu=promise.get_future();

    thread_=std::make_unique<std::thread>([self,p=std::move(promise)]()mutable{
        self->tid_=CurrentThread::tid();
        p.set_value();
        self->func_();
    });

    //必须等到对象中的线程拿到tid之后才可以返回
    fu.wait();
}


void Thread::join()
{
    assert(started_&&"Thread::join() called on a thread that has not been started.");
    assert(!joined_&&"Thread::join() called on a thread that has already been joined.");

    if(started_&&!joined_)
    {
        joined_=true;
        thread_->join();
    }
}


