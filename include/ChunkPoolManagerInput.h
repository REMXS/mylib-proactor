#pragma once


#include <vector>
#include <liburing.h>


#include "ChunkPool.h"

class IoUringLoop;


struct ChunkPoolManagerInput
{
    ChunkPool pool_;    //具体的内存池
    IoUringLoop& loop_;

    std::vector<Chunk>chunks_data_;      //所有chunk对象，chunk元数据,按序号排序

    //注册buffer ring所需要的配置选项
    io_uring_buf_ring* input_buf_ring_;
    size_t ring_size_;  //buffer ring 的占用的大小
    int input_buf_ring_mask_;
    io_uring_buf_reg reg_;
    uint16_t count_;

    ChunkPoolManagerInput(IoUringLoop& loop_);

    ~ChunkPoolManagerInput();


    Chunk* getChunkById(uint16_t id);

    void returnOneChunk(Chunk* chunk);

    inline uint16_t get_buf_group_id()const {return reg_.bgid;}
};