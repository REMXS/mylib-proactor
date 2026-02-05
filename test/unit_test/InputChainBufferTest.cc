#include "test_helper.h"
#include "ChunkPoolManagerInput.h"
#include "InputChainBuffer.h"
#include "IoUringLoop.h"

#include <liburing.h>
#include <memory>
#include <thread>
#include <cstring>

class InputChainBufferTest : public ::testing::Test
{
protected:
    inline static std::unique_ptr<IoUringLoop> loop_;
    inline static std::unique_ptr<ChunkPoolManagerInput> cpm_;
    std::unique_ptr<InputChainBuffer> icb_;

    static void SetUpTestSuite()
    {
        // 因为测试需要在一个线程中创建多个loop，所以要规避one loop per thread的检查
        std::thread t([&](){
            loop_ = std::make_unique<IoUringLoop>(1024, 32,1,4096,32);
        });
        t.join();
        cpm_ = std::make_unique<ChunkPoolManagerInput>(*loop_);
    }

    static void TearDownTestSuite()
    {
        cpm_.reset();
        loop_.reset();
    }

    void SetUp()
    {
        icb_ = std::make_unique<InputChainBuffer>(*cpm_);
    }

    void TearDown()
    {
        icb_.reset();
    }
};

// 测试1：空缓冲区的peek操作
TEST_F(InputChainBufferTest, PeekOnEmptyBuffer)
{
    auto [ptr, size] = icb_->peek();
    EXPECT_EQ(ptr, nullptr);
    EXPECT_EQ(size, 0);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试2：单个chunk的append和peek
TEST_F(InputChainBufferTest, AppendAndPeekSingleChunk)
{
    uint16_t index = 0;
    const char* test_data = "Hello";
    int data_len = 5;

    // 模拟获取chunk并设置数据
    icb_->append(index, data_len);

    auto [ptr, size] = icb_->peek();
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(size, data_len);
    EXPECT_EQ(icb_->getTotalLen(), data_len);
}

// 测试3：remove操作-部分数据读取
TEST_F(InputChainBufferTest, RemovePartialData)
{
    uint16_t index = 0;
    int data_len = 10;
    
    icb_->append(index, data_len);
    EXPECT_EQ(icb_->getTotalLen(), data_len);

    char buffer[5];
    size_t read = icb_->remove(buffer, 5);
    EXPECT_EQ(read, 5);
    EXPECT_EQ(icb_->getTotalLen(), 5);
}

// 测试4：remove操作-全部数据读取
TEST_F(InputChainBufferTest, RemoveAllData)
{
    uint16_t index = 0;
    int data_len = 10;
    
    icb_->append(index, data_len);

    char buffer[15];
    size_t read = icb_->remove(buffer, 15);
    EXPECT_EQ(read, data_len);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试5：retrieve操作
TEST_F(InputChainBufferTest, RetrieveData)
{
    uint16_t index = 0;
    int data_len = 20;
    
    icb_->append(index, data_len);
    EXPECT_EQ(icb_->getTotalLen(), data_len);

    icb_->retrieve(10);
    EXPECT_EQ(icb_->getTotalLen(), 10);

    icb_->retrieve(10);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试6：多个chunk的append
TEST_F(InputChainBufferTest, AppendMultipleChunks)
{
    uint16_t index1 = 0;
    uint16_t index2 = 1;
    int len1 = 10;
    int len2 = 15;

    icb_->append(index1, len1);
    EXPECT_EQ(icb_->getTotalLen(), len1);

    icb_->append(index2, len2);
    EXPECT_EQ(icb_->getTotalLen(), len1 + len2);
}

// 测试7：remove跨越多个chunk
TEST_F(InputChainBufferTest, RemoveAcrossMultipleChunks)
{
    icb_->append(0, 10);
    icb_->append(1, 15);
    icb_->append(2, 20);
    
    EXPECT_EQ(icb_->getTotalLen(), 45);

    char buffer[30];
    size_t read = icb_->remove(buffer, 30);
    EXPECT_EQ(read, 30);
    EXPECT_EQ(icb_->getTotalLen(), 15);
}

// 测试8：removeAsString操作
TEST_F(InputChainBufferTest, RemoveAsString)
{
    icb_->append(0, 5);
    
    std::string result = icb_->removeAsString(5);
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试9：removeAllAsString操作
TEST_F(InputChainBufferTest, RemoveAllAsString)
{
    icb_->append(0, 10);
    icb_->append(1, 15);
    
    std::string result = icb_->removeAllAsString();
    EXPECT_EQ(result.size(), 25);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试10：remove超过可用数据量
TEST_F(InputChainBufferTest, RemoveMoreThanAvailable)
{
    icb_->append(0, 10);
    
    char buffer[20];
    size_t read = icb_->remove(buffer, 20);
    EXPECT_EQ(read, 10);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试11：retrieve超过可用数据量
TEST_F(InputChainBufferTest, RetrieveMoreThanAvailable)
{
    icb_->append(0, 10);
    
    icb_->retrieve(20);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试12：交替的append和remove
TEST_F(InputChainBufferTest, InterleavedAppendAndRemove)
{
    icb_->append(0, 10);
    
    char buffer1[5];
    icb_->remove(buffer1, 5);
    EXPECT_EQ(icb_->getTotalLen(), 5);

    icb_->append(1, 15);
    EXPECT_EQ(icb_->getTotalLen(), 20);

    char buffer2[20];
    size_t read = icb_->remove(buffer2, 20);
    EXPECT_EQ(read, 20);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试13：peek不影响数据
TEST_F(InputChainBufferTest, PeekDoesNotModifyData)
{
    icb_->append(0, 10);
    size_t initial_size = icb_->getTotalLen();

    auto [ptr1, size1] = icb_->peek();
    auto [ptr2, size2] = icb_->peek();

    EXPECT_EQ(size1, size2);
    EXPECT_EQ(icb_->getTotalLen(), initial_size);
}

// 测试14：空缓冲区的remove
TEST_F(InputChainBufferTest, RemoveOnEmptyBuffer)
{
    char buffer[10];
    size_t read = icb_->remove(buffer, 10);
    EXPECT_EQ(read, 0);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}

// 测试15：空缓冲区的retrieve
TEST_F(InputChainBufferTest, RetrieveOnEmptyBuffer)
{
    icb_->retrieve(10);
    EXPECT_EQ(icb_->getTotalLen(), 0);
}