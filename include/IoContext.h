#pragma once
#include <iostream>


struct IoContext
{
    int res_;
    uint32_t flags_;    
    virtual void on_completion()=0;
};

