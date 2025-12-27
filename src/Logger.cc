#include"Logger.h"
#include"Timestamp.h"
#include<iostream>
Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level)
{
    this->log_level_=level;
}

void Logger::log(std::string msg)
{
    std::string pre="";
    switch (this->log_level_)
    {
    case LogLevel::INFO: 
        pre="[INFO]";
        break;
    case LogLevel::ERROR: 
        pre="[ERROR]";
        break;
    case LogLevel::FATAL:
        pre="[FATAL]";
        break;
    case LogLevel::DEBUG:
        pre="[DEBUG]";
        break;
    default:
        break;
    }
    std::cout<<pre+Timestamp::now().to_string()<<':'<<msg<<std::endl;
}