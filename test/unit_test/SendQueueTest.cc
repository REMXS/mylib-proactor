#include "test_helper.h"
#include "SendQueue.hpp"

TEST(SendQueueTest, AppendStringAndRetrieve)
{
    SendQueue q;
    std::string s = "hello";
    q.append(s);
    EXPECT_FALSE(q.isEmpty());

    auto iov = q.getOneIovec();
    ASSERT_EQ(iov.iov_len, s.size());
    EXPECT_EQ(std::string(static_cast<char*>(iov.iov_base), iov.iov_len), s);

    q.retrieve(3);
    EXPECT_FALSE(q.isEmpty());

    q.retrieve(2);
    EXPECT_TRUE(q.isEmpty());
}

TEST(SendQueueTest, AppendVectorAndCString)
{
    SendQueue q;
    std::vector<char> v{'a','b','c'};
    q.append(v);
    q.append("XY", 2);

    std::vector<iovec> iovecs;
    q.getBatchFragment(iovecs, 4);
    ASSERT_EQ(iovecs.size(), 2u);
    EXPECT_EQ(iovecs[0].iov_len, 3u);
    EXPECT_EQ(std::string(static_cast<char*>(iovecs[0].iov_base), 3), "abc");
    EXPECT_EQ(std::string(static_cast<char*>(iovecs[1].iov_base), 2), "XY");

    q.retrieve(5);
    EXPECT_TRUE(q.isEmpty());
}

TEST(SendQueueTest, PartialRetrieveKeepsState)
{
    SendQueue q;
    q.append("abcdef", 6);

    auto iov = q.getOneIovec();
    ASSERT_EQ(iov.iov_len, 6u);
    q.retrieve(4);

    auto iov2 = q.getOneIovec();
    ASSERT_EQ(iov2.iov_len, 2u);
    EXPECT_EQ(std::string(static_cast<char*>(iov2.iov_base), 2), "ef");

    q.retrieve(2);
    EXPECT_TRUE(q.isEmpty());
}

TEST(SendQueueTest, BatchLimitRespected)
{
    SendQueue q;
    q.append("123", 3);
    q.append("456", 3);
    q.append("789", 3);

    std::vector<iovec> iovecs;
    q.getBatchFragment(iovecs, 2);
    ASSERT_EQ(iovecs.size(), 2u);
    EXPECT_EQ(std::string(static_cast<char*>(iovecs[0].iov_base), 3), "123");
    EXPECT_EQ(std::string(static_cast<char*>(iovecs[1].iov_base), 3), "456");

    q.retrieve(6);
    EXPECT_FALSE(q.isEmpty());

    iovecs.clear();
    q.getBatchFragment(iovecs, 2);
    ASSERT_EQ(iovecs.size(), 1u);
    EXPECT_EQ(std::string(static_cast<char*>(iovecs[0].iov_base), 3), "789");
    q.retrieve(3);
    EXPECT_TRUE(q.isEmpty());
}

