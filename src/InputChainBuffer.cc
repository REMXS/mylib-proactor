#include "InputChainBuffer.h"
#include "ChunkPoolManagerInput.h"
#include "Logger.h"

bool InputChainBuffer::push_back(uint16_t index, int len)
{
    //从io_uring的cqe flags中获取内存编号，构建chunk并加入到缓冲区中
    auto new_chunk = chunk_pool_manager_.getChunkById(index);

    if(!tail_)
    {
        head_ = tail_ =new_chunk;
    }
    else
    {
        //更新尾节点
        tail_->next_ =new_chunk;
        tail_ = tail_->next_;
    }
    chunks_++;
    return true;
}

bool InputChainBuffer::pop_front()
{
    if(chunks_==0) return false;
    auto ret = head_;
    if(head_==tail_)
    {
        head_=tail_=nullptr;
    }
    else
    {
        head_=head_->next_;
    }

    chunk_pool_manager_.returnOneChunk(ret);

    chunks_--;
    return true;
}

InputChainBuffer::InputChainBuffer(ChunkPoolManagerInput &chunk_pool_manager)
    :chunk_pool_manager_(chunk_pool_manager)
    ,head_(nullptr)
    ,tail_(nullptr)
    ,chunks_(0)
    ,total_len_(0)
{}

InputChainBuffer::~InputChainBuffer()
{
    //归还所有的内存块
    while(chunks_)
    {
        pop_front();
    }
}

size_t InputChainBuffer::remove(char *target, size_t len)
{
    size_t read_count=0;

    while(read_count<len&&head_)
    {
        char* seg_start = target+read_count;
        //1.读取数据
        size_t fill_size = std::min(head_->readableBytes(),len-read_count);
        //拷贝数据并偏移指针
        std::copy(head_->beginRead(),head_->beginRead()+fill_size,seg_start);
        head_->head_+=fill_size;
        read_count+=fill_size;

        //2.如果chunk完全空了，则归还这个chunk
        if(head_->readableBytes()==0)
        {
            pop_front();
        }  
    }
    
    LOG_DEBUG("chunks : %lu",chunks_);
    total_len_-=read_count;
    return read_count;
}

std::string InputChainBuffer::removeAsString(size_t size)
{
    std::string ret(size,0);
    size_t len = remove(ret.data(),ret.size());
    ret.resize(len);
    return ret;
}

std::string InputChainBuffer::removeAllAsString()
{
    return removeAsString(total_len_);
}

std::pair<char *,size_t> InputChainBuffer::peek()
{
    if(!head_) return {nullptr,0};
    return {head_->beginRead(),head_->readableBytes()};
}

void InputChainBuffer::retrieve(size_t len)
{
    size_t read_bytes = 0;
    while(read_bytes<len&&head_)
    {
        size_t offset = std::min(head_->readableBytes(),len-read_bytes);
        head_->head_+=offset;
        read_bytes += offset;
        if(head_->readableBytes()==0)
        {
            pop_front();
        }
    }
    total_len_ -= read_bytes;
}

// 这里的index 是cqe返回的buffer ring中内存块的下标
void InputChainBuffer::append(uint16_t index, int len)
{
    push_back(index,len);
    //写入数据信息
    tail_->tail_+=len;
    total_len_ += len;
}
