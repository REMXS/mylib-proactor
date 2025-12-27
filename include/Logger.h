#pragma once
#include<string>
#include"noncopyable.h"

//##__VA_ARGS__ 中的##表示在参数包为空的时候，自动消除它前面的一个 ","。

#define LOG_INFO(logmsgFormat,...)                          \
    do                                                      \
    {                                                       \
        Logger&logger=Logger::getInstance();                \
        logger.setLogLevel(LogLevel::INFO);                 \
        char buffer[1024]={0};                              \
        snprintf(buffer,1024,logmsgFormat,##__VA_ARGS__);   \
        logger.log(buffer);                                 \
    } while (0);                                            \
    


#define LOG_ERROR(logmsgFormat,...)                         \
    do                                                      \
    {                                                       \
        Logger&logger=Logger::getInstance();                \
        logger.setLogLevel(LogLevel::ERROR);                \
        char buffer[1024]={0};                              \
        snprintf(buffer,1024,logmsgFormat,##__VA_ARGS__);   \
        logger.log(buffer);                                 \
    } while (0);                                            \


//发生错误，记录并退出程序
#define LOG_FATAL(logmsgFormat,...)                         \
    do                                                      \
    {                                                       \
        Logger&logger=Logger::getInstance();                \
        logger.setLogLevel(LogLevel::FATAL);                \
        char buffer[1024]={0};                              \
        snprintf(buffer,1024,logmsgFormat,##__VA_ARGS__);   \
        logger.log(buffer);                                 \
        exit(-1);                                           \
    } while (0);                                            \


#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat,...)                         \
    do                                                      \
    {                                                       \
        Logger&logger=Logger::getInstance();                \
        logger.setLogLevel(LogLevel::DEBUG);                \
        char buffer[1024]={0};                              \
        snprintf(buffer,1024,logmsgFormat,##__VA_ARGS__);   \
        logger.log(buffer);                                 \
    } while (0);                                            \

#else
#define LOG_DEBUG(logmsgFormat,...)
#endif






enum class LogLevel
{
    INFO, //普通信息
    ERROR, //错误信息
    FATAL, //core dump 信息
    DEBUG //调试信息
};


//单例类
class Logger:noncopyable
{
private:
    LogLevel log_level_;
    Logger(/* args */)=default;
    ~Logger()=default;
public:
    static Logger& getInstance();
    //设置日志等级
    void setLogLevel(LogLevel level);
    //输出日志
    void log(std::string msg);

};

