#pragma once
#include <queue>
#include <arpa/inet.h>

#include "IoContext.h"


struct AcceptContext: public IoContext
{
    std::queue<int>sock_fds_;
    sockaddr_in addr_;     //监听的地址 
    int fd_;

    void on_completion()override;
};