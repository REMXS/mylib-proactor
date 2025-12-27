#pragma once

#include<unistd.h>
#include<sys/syscall.h>


//因为获取tid需要系统调用资源开销比较大，所以这个类的作用就是缓存tid
namespace CurrentThread
{
    extern thread_local int t_cachedTid;

    void cacheTid();

    inline int tid()
    {
        //__builtin_expect 的意思是 告诉编译器t_cachedTid==0 的值很可能是false，也就是t_cachedTid的值很可能不为0，这个是一项指导编译器优化
        if(__builtin_expect(t_cachedTid==0,0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
} // namespace CurrentThread
