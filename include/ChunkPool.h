#pragma once
#include <iostream>
#include <sys/mman.h>


//注意：io_uring中的buffer ring 大小必须是2的幂
constexpr size_t CHUNK_SIZE = 4096; // 4KB
constexpr size_t POOL_SIZE = 4096*64; // 64MB 预注册内存

//链表的节点
struct Chunk
{
    char* data_ptr_;    //具体的内存地址
    uint16_t index_;    //这个内存的编号
    Chunk* next_=nullptr; //链表指针，指向下一个chunk的地址
    size_t head_=0;       //数据起始的偏移量
    size_t tail_=0;       //数据结束时的偏移量

    Chunk(char* data_ptr,uint16_t idx)
        :data_ptr_(data_ptr)
        ,index_(idx)
    {}

    size_t capacity()const {return CHUNK_SIZE;}
    size_t readableBytes()const {return tail_-head_;}
    size_t writableBytes()const {return CHUNK_SIZE-tail_;}
    void reset() {head_=tail_=0;next_=nullptr;}

    //返回可写位置的指针
    char* beginWrite()const {return data_ptr_+tail_;}
    //返回可读位置的指针
    char* beginRead()const {return data_ptr_+head_;}
}; 


struct ChunkPool
{
    char* data_ptr_;        //内存的起始地址
    const size_t bytes_;    //内存池的总内存大小
    const size_t chunks_;   //内存的块数

    ChunkPool()
        :bytes_(POOL_SIZE)
        ,chunks_(POOL_SIZE/CHUNK_SIZE)
        ,data_ptr_(nullptr)
    {
        //TODO 使用mmap分配地址，MAP_LOCKED为锁定内存，防止swap
        int flags = MAP_ANONYMOUS|MAP_PRIVATE|MAP_HUGETLB|MAP_LOCKED;
        data_ptr_ = (char*)mmap(nullptr,bytes_,PROT_READ|PROT_WRITE,flags,-1,0);
        //如果MAP_HUGETLB失败，则不开启大页优化
        if(data_ptr_ ==MAP_FAILED)
        {
            std::cerr<<"Warning: Huge pages failed, falling back to standard pages.\n";
            flags&= ~MAP_HUGETLB;
            data_ptr_ = (char*)mmap(nullptr,bytes_,PROT_READ|PROT_WRITE,flags,-1,0);
            if (data_ptr_ == MAP_FAILED) {
                perror("mmap failed");
                exit(1);
            }
        }
    }
    ~ChunkPool()
    {
        //TODO 使用munmap释放地址
        if(data_ptr_&&data_ptr_!=MAP_FAILED)
        {
            munmap(data_ptr_,bytes_);
        }
    }
};