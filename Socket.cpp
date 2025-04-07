#include "Socket.h"
#include <unistd.h>
#include "Logger.h"
#include <sys/types.h>
#include <sys/socket.h
#include "InetAddress.h"
// 指定文件描述符关闭连接
Socket::~Socket(){
    ::close(sockfd_);
}

/* int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
  使用sys函数将一个套接字（socket）和一个特定的地址及端口绑定在一起
  sockfd：代表一个已经创建好的套接字描述符 该描述符由 ::socket 函数返回
  addr：是一个指向 struct sockaddr 类型的指针 
  ***struct sockaddr_in 用于 IPv4再将其强制转换为 struct sockaddr 类型***
  addrlen：表示 addr 所指向的地址结构的长度
*/
void Socket::bindAddress(const InetAddress& localaddr){
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in) )){
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
    }   
}

/*
 开启socket监听事件
*/
void Socket::listen(){
    if (0 != ::listen(sockfd_, 1024)){
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
    }
}

/*
::accept 是一个系统调用函数，用于从监听套接字 sockfd_ 接受一个新的客户端连接
&len：传递 len 变量的地址 用于存储 ***客户端地址*** 结构体的长度
connfd：存储新建立连接的套接字描述符
*/
int Socket::accept(InetAddress* peeraddr){
    sockaddr_in addr;
    socklen_t len;
    bzero(&addr, sizeof addr);
    /*
     返回的是服务器本体的一个端口对应的文件描述符
     也就是服务器开启的一个端口 用于和客户端通信
    */
    int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
    if (connfd >= 0){
        // 将接收到的客户端的结构体存储到 InetAddress 中
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}



void Socket::shutdownWrite(); {
    if (::shutdown(sockfd_, SHUT_WR) < 0){
        LOG_ERROR("shutdownWrite error");
    }
}

void Socket::setTcpNoDelay(bool on){
    int optval = on ? 1 : 0;
    // 使用 setsockopt 函数设置 TCP_NODELAY 选项
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on){
    int optval = on ? 1 : 0;
    // 使用 setsockopt 函数设置 SO_REUSEADDR 选项
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
}


void Socket::setReusePort(bool on){
    int optval = on ? 1 : 0;
    // 使用 setsockopt 函数设置 SO_REUSEPORT 选项
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))
}


void Socket::setKeepAlive(bool on){
    int optval = on ? 1 : 0;
    // 使用 setsockopt 函数设置 SO_KEEPALIVE 选项
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))
}
