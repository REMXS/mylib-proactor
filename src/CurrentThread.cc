#include"CurrentThread.h"



#if defined(__linux__)
namespace CurrentThread
{
    thread_local int t_cachedTid=0;

    void cacheTid()
    {   
        if(!t_cachedTid)
        {
            t_cachedTid=static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
} // namespace CurrentThread

#elif defined(__APPLE__)
#include<pthread.h>
namespace CurrentThread
{
    thread_local int t_cachedTid=0;

    void cacheTid()
    {   
        if(!t_cachedTid)
        {
            pthread_threadid_np(NULL, &t_cachedTid);
        }
    }
} // namespace CurrentThread

#endif