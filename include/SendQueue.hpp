#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <deque>
#include <memory>
#include <cstring>
#include <sys/uio.h>
#include <type_traits>
#include <utility>

//可选扩展：对象池扩展，但是要考虑背压问题或者是使用动态扩展策略

//一个逻辑数据分片
struct TxFragment
{
    const char* ptr_;   //指向原始数据的指针
    size_t len_;        //这块数据的大小
    size_t written_;    //已经发送出去的数据的大小

    std::shared_ptr<void>guard_;

    TxFragment* next_;
    
    TxFragment()
        :ptr_(nullptr)
        ,len_(0)
        ,written_(0)
        ,guard_(nullptr)
        ,next_(nullptr) 
    {}

    const char* beginRead()const {return ptr_+written_;}
    size_t remainData()const {return len_-written_;}
};

class SendQueue
{
private:
    TxFragment* head_;
    TxFragment* tail_;
    TxFragment* curr_;

    size_t slice_size_;//链表的大小
    size_t total_len_; //数据的总大小

    void push_back()
    {
        auto fragment = new TxFragment();

        if(!tail_)
        {
            head_=curr_=tail_=fragment;
        }
        else
        {
            tail_->next_ =fragment;
            tail_ = fragment;
        }
        //如果之前数据都发送了但是还没发完，这时要更新curr_指针
        if(!curr_) curr_=tail_;
        slice_size_++;
    }  

    void pop_front()
    {
        if(!head_) return;
        auto del_fragment = head_;
        
        head_=head_->next_;
        if(!head_)
        {
            curr_=tail_=nullptr;
        }

        delete del_fragment;
        slice_size_--;
    }

public:
    SendQueue()
        : head_(nullptr)
        , tail_(nullptr)
        , curr_(nullptr)
        , slice_size_(0)
        , total_len_(0)
    {}

    ~SendQueue()
    {
        while (head_) pop_front();
    }

    // C++20: 仅接受拥有内存且连续的缓冲类型
    template <typename Buffer>
    //移除const volatile 修饰
    requires (
        std::is_same_v<std::remove_cvref_t<Buffer>, std::string> ||
        std::is_same_v<std::remove_cvref_t<Buffer>, std::vector<char>>
    )
    void append(Buffer&& data)
    {
        push_back();
        using T = std::remove_cvref_t<Buffer>;
        auto holder = std::make_shared<T>(std::forward<Buffer>(data));
        tail_->ptr_ = holder->data();
        tail_->len_ = holder->size();
        tail_->guard_ = std::move(holder);

        total_len_ += data.size();
    }
    void append(const char*data,size_t len)
    {
        push_back();
        auto holder = std::make_shared<std::string>(data, len);
        tail_->ptr_ = holder->data();
        tail_->len_ = holder->size();
        tail_->guard_ = std::move(holder);

        total_len_ += len;
    }


    iovec getOneIovec()
    {
        if(!curr_) return iovec{};
        iovec ret{};
        ret.iov_base = (void*)curr_->beginRead();
        ret.iov_len =curr_->remainData();
        curr_ = curr_->next_;
        return ret;
    }
    void getBatchFragment(std::vector<iovec>&iovecs,size_t max_count)
    {
        size_t i =0;
        while(i<max_count&&curr_)
        {
            iovecs.emplace_back(getOneIovec());
            i++;
        }
        iovecs.resize(i);
    }

    void retrieve(size_t len)
    {
        size_t written = 0;
        while(written<len&&head_)
        {
            size_t offset = std::min(head_->remainData(),len-written);
            head_->written_+=offset;
            written += offset;
            if(head_->written_==head_->len_)
            {
                pop_front();
            }
        }
        curr_ = head_;
        total_len_ -= written;
    }

    bool isEmpty()const {return total_len_==0;}

};
