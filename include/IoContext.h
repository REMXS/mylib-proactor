#pragma once
#include <iostream>
#include <concepts>

enum class ContextType : uint8_t {
    Read,
    Write,
    Accept,
    Wakeup
};

struct IoContext
{
    ContextType type_;
    int res_;
    uint32_t flags_;
    IoContext(ContextType t) : type_(t), res_(0), flags_(0) {}
};

