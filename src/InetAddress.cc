#include<cstring>

#include"InetAddress.h"

InetAddress::InetAddress(uint16_t port,std::string ip)
{
    memset(&addr_,0,sizeof(addr_));
    addr_.sin_family=AF_INET;
    addr_.sin_addr.s_addr=::inet_addr(ip.c_str());
    addr_.sin_port=htons(port);
}

InetAddress::InetAddress(const sockaddr_in&addr)
    :addr_(addr)
{
}

InetAddress::~InetAddress()
{
}

void InetAddress::setSockAddr(const sockaddr_in&addr)
{
    addr_=addr;    
}

std::string InetAddress::toIp()const
{
    char buf[64]={0};
    inet_ntop(AF_INET,&addr_.sin_addr.s_addr,buf,sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort()const
{
    char buf[64]={0};
    inet_ntop(AF_INET,&addr_.sin_addr.s_addr,buf,sizeof(buf));
    size_t end=strlen(buf);
    snprintf(buf+end,sizeof(buf)-end,":%u",ntohs(addr_.sin_port));
    return buf;
}

uint16_t InetAddress::toPort()const
{
    return ntohs(addr_.sin_port);
}

/* 
在 const 成员函数中，成员变量的指针不会“自动加上 const 修饰”，
但由于 this 指针是 const T*，通过 this 访问的成员变量指针会
被推导为指向 const 的指针类型。
*/
const sockaddr_in* InetAddress::getSockAddr()const
{
    return &addr_;
}