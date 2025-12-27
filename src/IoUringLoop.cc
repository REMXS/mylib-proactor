#include <sys/eventfd.h>


#include "IoUringLoop.h"
#include "Logger.h"

//防止一个线程创建多个eventloop
//因为这个变量仅供内部判断使用，所以定义在实现文件，不对外暴露
//对外使用new 创建的eventloop对象而不使用t_loopInThisThread
thread_local IoUringLoop* t_loopInThisThread=nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs=10000; //单位为ms ，10000为10s


/*  创建一个eventfd
    EFD_NONBLOCK 表示设置为非阻塞
    EFD_CLOEXEC 表明在执行exec函数创建新进程的时候，不继承此fd
    因为eventfd用于进程中线程之间的通信，创建新进程线程的上下文不存在，
    所以eventfd要关闭防止意外的行为
 */
int createEventFd()
{
    int evtfd=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(evtfd<0)
    {
        LOG_FATAL("failed to create eventfd,errno: %d",errno)
    }
    LOG_DEBUG("successed to create eventfd %d",evtfd)
    return evtfd;
}


