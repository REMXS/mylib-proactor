#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include <sys/signal.h>

struct IgnoreSigPipe
{
    IgnoreSigPipe()
    {
        struct sigaction sa;
        sa.sa_handler = SIG_IGN;  // 忽略
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        
        if (sigaction(SIGPIPE, &sa, NULL) == -1) {
            perror("sigaction");
            exit(-1);
        }
    }
};

int main()
{
    IgnoreSigPipe();

    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}