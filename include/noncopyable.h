#pragma once


//继承此类的类都无法复制和拷贝构造，但是可以正常构造和析构
class noncopyable
{
public:
    noncopyable& operator=(const noncopyable&other)=delete;
    noncopyable(const noncopyable&other)=delete;
protected: //设置为protected 确保此类不会被单独实例化
    noncopyable()=default;
    ~noncopyable()=default;
};

