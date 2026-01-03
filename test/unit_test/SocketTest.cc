#include<arpa/inet.h>
#include<fcntl.h>
#include<error.h>

#include "test_helper.h"
#include"Logger.h"
#include"Socket.h"
#include"InetAddress.h"

TEST(SocketTest,BaseTest)
{
    int lfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    int flags = fcntl(lfd, F_GETFL, 0);
    fcntl(lfd, F_SETFL, flags | O_NONBLOCK);
    InetAddress ia(9999);
    Socket so(lfd);
    so.bindAddress(ia);

    InetAddress conn_addr;
    ASSERT_EQ(so.accept(conn_addr),-1);
    ASSERT_EQ(errno,EINVAL);
    so.listen();
    ASSERT_EQ(so.accept(conn_addr),-1);
    ASSERT_EQ(errno,EWOULDBLOCK);


    so.setKeepAlive(true);
    so.setReuseAddr(true);
    so.setReusePort(true);
    so.setTcpNoDelay(true);
    so.shutDownWrite();
}