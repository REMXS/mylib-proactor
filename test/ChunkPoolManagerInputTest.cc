#include "test_helper.h"
#include "ChunkPoolManagerInput.h"
#include "IoUringLoop.h"

#include <liburing.h>
#include <memory>
#include <thread>

class ChunkPoolManagerInputTest : public ::testing::Test
{
protected:
    std::unique_ptr<IoUringLoop> loop_;
    std::unique_ptr<ChunkPoolManagerInput> cpm_;

    void SetUp() override
    {
        //因为测试需要在一个线程中创建多个loop，所以要规避one loop per thread的检查
        std::thread t([&](){
            loop_ = std::make_unique<IoUringLoop>(1024,32);
        });
        t.join();
        cpm_ = std::make_unique<ChunkPoolManagerInput>(*loop_);
    }

    void TearDown() override
    {
        cpm_.reset();
        loop_.reset();
    }
};

TEST_F(ChunkPoolManagerInputTest, ConstructorInitializesCorrectly)
{
    ASSERT_NE(cpm_, nullptr);
    EXPECT_NE(cpm_->input_buf_ring_, nullptr);
    EXPECT_GT(cpm_->reg_.ring_entries, 0u);
    EXPECT_GT(cpm_->get_buf_group_id(), 0);
    EXPECT_EQ(cpm_->chunks_data_.size(), cpm_->pool_.chunks_);
}

TEST_F(ChunkPoolManagerInputTest, GetChunkByIdValid)
{
    uint16_t id = 0;
    Chunk* chunk = cpm_->getChunkById(id);
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->index_, id);
    EXPECT_NE(chunk->data_ptr_, nullptr);
}

TEST_F(ChunkPoolManagerInputTest, GetChunkByIdInvalid)
{
    uint16_t invalid_id = cpm_->chunks_data_.size() + 10;
    Chunk* chunk = cpm_->getChunkById(invalid_id);
    EXPECT_EQ(chunk, nullptr);
}

TEST_F(ChunkPoolManagerInputTest, ReturnOneChunkResetsState)
{
    Chunk* chunk = cpm_->getChunkById(0);
    ASSERT_NE(chunk, nullptr);

    // 模拟使用：写入数据
    chunk->tail_ = 8;
    
    cpm_->returnOneChunk(chunk);
    
    // 验证 reset 被调用
    EXPECT_EQ(chunk->tail_, 0u);
}

TEST_F(ChunkPoolManagerInputTest, ReturnMultipleChunksBatchCommit)
{
    // 归还多个 chunk，触发批量提交
    for (int i = 0; i < 35; ++i)
    {
        Chunk* chunk = cpm_->getChunkById(i % cpm_->chunks_data_.size());
        ASSERT_NE(chunk, nullptr);
        chunk->tail_ = 10;
        cpm_->returnOneChunk(chunk);
    }
    
    // 验证计数器被重置（批量提交后）
    EXPECT_LT(cpm_->count_, 33u);
}

TEST_F(ChunkPoolManagerInputTest, AllChunksHaveUniqueIndices)
{
    std::set<uint16_t> indices;
    for (const auto& chunk : cpm_->chunks_data_)
    {
        EXPECT_TRUE(indices.insert(chunk.index_).second) 
            << "Duplicate index found: " << chunk.index_;
    }
    EXPECT_EQ(indices.size(), cpm_->chunks_data_.size());
}

TEST_F(ChunkPoolManagerInputTest, BufferRingMaskCalculation)
{
    int expected_mask = io_uring_buf_ring_mask(cpm_->pool_.chunks_);
    EXPECT_EQ(cpm_->input_buf_ring_mask_, expected_mask);
}



