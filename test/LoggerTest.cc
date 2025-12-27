#include "test_helper.h"
#include "Logger.h"

TEST(LoggerTest, BasicLogging){
    LOG_INFO("hello,world")
    LOG_INFO("this is %d",199)
    LOG_INFO("%d,%s",123,"hel")
    LOG_ERROR("hello,world")
    LOG_ERROR("this is %d",199)
    LOG_ERROR("%d,%s",123,"hel")
    EXPECT_EXIT(LOG_FATAL("hello,world"),::testing::ExitedWithCode(255), "");
    EXPECT_EXIT(LOG_FATAL("this is %d",199),::testing::ExitedWithCode(255), "");
    EXPECT_EXIT(LOG_FATAL("%d,%s",123,"hel"),::testing::ExitedWithCode(255), "");
    LOG_DEBUG("hello,world")
    LOG_DEBUG("this is %d",199)
    LOG_DEBUG("%d,%s",123,"hel")
}