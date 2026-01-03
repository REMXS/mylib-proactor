#include "test_helper.h"
#include "Timestamp.h"

TEST(TimestampTest, Basic){
    Timestamp ts;
    ts=Timestamp::now();
    EXPECT_NEAR(0,
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()-ts.microSecondsSinceEpoch(),20);
    Timestamp ts2(ts);
    Timestamp ts3(time(NULL));

    Timestamp ts4=ts3;
    ts4=ts3;
    EXPECT_EQ(ts3,ts4);
}

TEST(TimestampTest, ToString){
    Timestamp ts(Timestamp::now());
    std::string str=ts.to_string();
    std::cout<<str<<std::endl;
}

TEST(TimestampTest, Comparison){
    Timestamp ts1(1000);
    Timestamp ts2(2000);
    EXPECT_LT(ts1,ts2);
    EXPECT_GT(ts2,ts1);
    EXPECT_NE(ts1,ts2);
    EXPECT_EQ(ts1,ts1);
}
TEST(TimestampTest,addTime_test)
{
    Timestamp ts1(1000000); // 1 second
    double secondsToAdd = 2.5; // 2.5 seconds
    Timestamp ts2 = addTime(ts1, secondsToAdd);
    EXPECT_EQ(ts2.microSecondsSinceEpoch(), 3500000); // 3.5 seconds in microseconds
}