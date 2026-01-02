#pragma once
#include <iostream>
#include <string_view>

struct Chunk;
class ChunkPoolManagerInput;
class TcpConnection;

//输入缓冲区，生产者为内核，消费者为用户
class InputChainBuffer
{
private:
    Chunk* head_;       //链表的头节点
    Chunk* tail_;       //链表的尾节点
    size_t chunks_;     //链表的大小
    size_t total_len_;  //数据的总长度

    
    ChunkPoolManagerInput&chunk_pool_manager_;

    bool push_back(uint16_t index,int len);//这里len用int类型的是因为io_uring中cqe res字段的类型为signed int

    //弹出头节点并把头节点返回给内存池
    bool pop_front();

    Chunk* front()const {return head_;}
    Chunk* back()const {return tail_;}
    
public:
    InputChainBuffer(ChunkPoolManagerInput&chunk_pool_manager);

    ~InputChainBuffer();

    //获取数据
    size_t remove(char* target,size_t len);

    std::string removeAsString(size_t size);

    std::string removeAllAsString();

    //查看第一个数据块的数据
    std::pair<char*,size_t> peek();

    //自己手动移动指针来消费数据
    void retrieve(size_t len);

    void append(uint16_t index,int len);

    size_t getTotalLen()const {return total_len_;}

    size_t getTotalChunk()const {return chunks_;}

};