#include<thread>
#include<memory>
#include<future>
#include<chrono>
#include "test_helper.h"

#include"Logger.h"
#include"Thread.h"


std::future<bool> test_thread_func()
{   
    //shared_ptr默认构造函数不会创建目标对象，所以不要忘了使用make_shared创建对象
    std::shared_ptr<std::promise<bool>>pr=std::make_shared<std::promise<bool>>();
    std::future<bool>ret=pr->get_future();
    /* std::promise 不可拷贝，只能移动，但 std::function 要求其目标可拷贝构造。
    当 lambda 捕获 std::promise（即使用 std::move）后，
    这个 lambda 变成了只能移动的类型，无法存储到 std::function 中。
    要使用 std::shared_ptr 包装 std::promise，使其可拷贝 */
    std::shared_ptr<Thread> t=std::make_shared<Thread>([pr]()->void {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout<<"this is a function and after the object it belongs still alive"<<std::endl;
        pr->set_value(true);
    },"my thread");
    t->start();

    EXPECT_EQ(t->isStarted(),true);
    std::cout<<t->name()<<" "<<t->numCreated<<std::endl;
    return ret;
}

TEST(ThreadTest, BasicThreadFunction)
{
    std::future<bool>fu = std::move(test_thread_func());
    fu.wait();
    EXPECT_EQ(fu.get(),true);
}

TEST(ThreadTest, JoinBeforeStart) {
    // EXPECT_DEATH 宏
    // 第一个参数：要执行的代码块
    // 第二个参数：一个正则表达式，用来匹配程序终止时输出到 stderr 的错误信息
    ASSERT_DEATH({
        std::shared_ptr<Thread> t =std::make_shared<Thread>([]{}); // 创建一个 Thread 对象
        t->join();       // 在 start() 之前调用 join()，这应该会触发 assert
    }, "Thread::join().*started"); // 检查 assert 的消息是否匹配

}

