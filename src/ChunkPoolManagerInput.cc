#include <cstring>

#include "ChunkPoolManagerInput.h"
#include "IoUringLoop.h"

uint16_t generateBid()
{
    static uint16_t bid=1;
    return bid++;
}

ChunkPoolManagerInput::ChunkPoolManagerInput(IoUringLoop &loop)
    :loop_(loop)
    ,pool_()
    ,count_(0)
{
    //分割内存成chunk，然后注册buffer ring
    chunks_data_.resize(pool_.chunks_,{0,0});

    for(int i=0;i<pool_.chunks_;++i)
    {
        chunks_data_[i]=Chunk(pool_.data_ptr_+CHUNK_SIZE*i,i);
    }

    //注册buffer ring

    //分配buffer ring 的内存
    //检测chunk的数量是否为2的幂
    if ((pool_.chunks_ & (pool_.chunks_ - 1)) != 0) {
        std::cerr << "pool_.chunks_ must be power of two for buf ring\n";
        exit(1);
    }
    
    //注意大小
    ring_size_ = sizeof(struct io_uring_buf_ring) +pool_.chunks_ * sizeof(struct io_uring_buf);
    void* ring_mem = mmap(nullptr, ring_size_, PROT_READ | PROT_WRITE, 
                            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (ring_mem == MAP_FAILED) {
        perror("mmap ring failed");
        exit(1);
    }

    //获取input_buf_ring_
    input_buf_ring_ = (io_uring_buf_ring *)ring_mem;
    // 【缺失的步骤】初始化 buffer ring 的控制结构（主要是 tail）
    io_uring_buf_ring_init(input_buf_ring_);
    
    //获取掩码
    input_buf_ring_mask_ = io_uring_buf_ring_mask(pool_.chunks_);

    //配置io_uring_buf_reg 相关信息
    //这里一定要记得清零，否则reg_中的一些变量的值就是随机的一些数，在注册buffer ring时就会出现invaild argument错误
    memset(&reg_, 0, sizeof(reg_));
    reg_.ring_addr = (uint64_t)input_buf_ring_;
    reg_.ring_entries = pool_.chunks_;
    reg_.bgid = generateBid();
    reg_.flags = 0;

    int ret = io_uring_register_buf_ring(loop_.ring_,&reg_,0);

    if(ret<0)
    {
        //处理错误
        std::cerr<<"io_uring_register_buf_ring failed "<<strerror(-ret)<<'\n';
        exit(1);
    }

    //初始填充数据块
    for(auto&chunk:chunks_data_)
    {
        io_uring_buf_ring_add(
            input_buf_ring_,
            chunk.data_ptr_,
            CHUNK_SIZE,
            chunk.index_,
            input_buf_ring_mask_,
            chunk.index_
        );
    }
    //提交
    io_uring_buf_ring_advance(input_buf_ring_,reg_.ring_entries);
}

ChunkPoolManagerInput::~ChunkPoolManagerInput()
{
    //取消注册buffer_ring
    if(loop_.ring_)
    {
        io_uring_unregister_buf_ring(loop_.ring_,reg_.bgid);
    }
    if(input_buf_ring_)
    {
        munmap(input_buf_ring_, ring_size_);
    }
}

Chunk *ChunkPoolManagerInput::getChunkById(uint16_t id)
{
    if(id>=chunks_data_.size()) return nullptr;
    return &chunks_data_[id];
}

void ChunkPoolManagerInput::returnOneChunk(Chunk *chunk)
{
    chunk->reset();
    //向io_uring 中归还这个获取的地址
    io_uring_buf_ring_add(
        this->input_buf_ring_,
        chunk->data_ptr_,
        CHUNK_SIZE,
        chunk->index_,
        this->input_buf_ring_mask_,
        count_
    );
    count_++;
    //如果计数器达到了一定次数则批量提交一次
    //FIXME: 之前是32，但是由于总共只有64个chunk，导致大量buffer被滞留，引发ENOBUFS
    if(count_>=1)
    {
        io_uring_buf_ring_advance(this->input_buf_ring_,count_);
        count_=0;
    }
}
