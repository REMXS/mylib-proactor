#pragma once
#include <iostream>
#include <coroutine>


template <typename T=void>
struct Task
{
    struct promise_type;
    std::coroutine_handle<promise_type>handle_;


    Task(std::coroutine_handle<promise_type>handle)
        :handle_(handle)
    {}
    ~Task()
    {
        if(handle_)
        {
            handle_.destroy();
        }
    }
    struct promise_type
    {
        T curr_value;
        Task<T> get_return_object()
        {
            return Task<T>{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend()
        {
            return {};
        }

        std::suspend_always final_suspend()noexcept
        {
            return {};
        }

        void return_value(){}
        T yield_value(T&&value)
        {
            curr_value =std::forward<T>(value);
        }

        void unhandled_exception()
        {
            try
            {
                std::rethrow_exception(std::current_exception());
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            
        }
        
    };

};

//特化一个void版本
template <>
struct Task<void>
{
    struct promise_type;
    std::coroutine_handle<promise_type>handle_;


    Task(std::coroutine_handle<promise_type>handle)
        :handle_(handle)
    {}
    ~Task()
    {
        if(handle_)
        {
            handle_.destroy();
        }
    }
    
    struct promise_type
    {
        Task<void> get_return_object()
        {
            return Task<void>{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend()
        {
            return {};
        }

        std::suspend_always final_suspend()noexcept
        {
            return {};
        }

        void return_value(){}


        void unhandled_exception()
        {
            try
            {
                std::rethrow_exception(std::current_exception());
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            
        }
        
    };

};