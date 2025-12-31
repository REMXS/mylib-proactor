#pragma once
#include <queue>
#include <arpa/inet.h>
#include <memory>

#include "IoContext.h"

class Acceptor;
struct AcceptContext: public IoContext
{
    AcceptContext(sockaddr_in addr,int fd);
    ~AcceptContext();
    //因为acceptor生命周期与整个进程的生命周期一致，所以不需要担心sqe失效
    sockaddr_in addr_;     //监听的地址 
    int fd_;

    bool is_error_;         //标记是否出错
    bool is_accepting_;     //标记是否正在运行
    
    Acceptor* acceptor_;

    bool handleError();

    void on_completion()override;
};