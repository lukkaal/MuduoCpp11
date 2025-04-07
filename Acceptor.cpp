#include "Acceptor.h"
#include <sys/types.h>
#include <sys/socket.h>
/*
 文件内 static 关键字用于修饰函数时 会将函数的作用域限制在当前文件内
 ::socket 是一个系统调用函数，用于创建一个套接字。
 AF_INET 表示使用 IPv4 地址族。
 SOCK_STREAM 表示创建一个面向连接的 TCP 套接字。
 SOCK_NONBLOCK 表示将套接字设置为非阻塞模式。
 SOCK_CLOEXEC 表示在执行 exec 系列函数时自动关闭该套接字，避免子进程继承该套接字。
 IPPROTO_TCP 表示使用 TCP 协议。
*/
static int createNonblocking(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | 
        SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0){
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, 
            __FUNCTION__, __LINE__， errno);
    }

}

Acceptpr::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking()) // 创建 Socket 类型 使用一个 sockfd 初始化
    /*
     说明此时的 sockfd 被 mainloop 当中的 poller 管理
    */
    , acceptChannel_(loop, acceptSocket_.fd()) // Channel 类型 使用当前的 EventLoop* (mainloop) 和 sockfd
    , listening_(false){
        acceptSocket_.setReuseAddr(true);
        acceptSocket_.setReusePort(true);
        acceptSocket_.bindAddress(listenAddr); // 用于绑定套接字监听的位置 还没有开启
        /*
         TcpServer.start()-> Acceptor.listen() -> 开启监听
         当有新用户的连接时 要执行一个读回调函数
         所以设置 sockfd 对应 Channel 的读回调
        */ 
        acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

    }


Acceptpr::~Acceptor(){
    acceptChannel_.disableAll(); // 将 Channel中的 event 设置为 都不关心
    acceptChannel_.remove(); // 从EpollPoller中删掉(ChannelMap 和 Epoll)
}

void Acceptpr::listen(){
    listening_ = true;
    /*
     一旦套接字处于监听状态 服务器就开始等待客户端的连接请求
     listen这些请求会被放入一个队列中（1024）等待服务器处理

     1.操作系统会为该套接字创建一个连接请求队列（也称为半连接队列和全连接队列）
     半连接队列用于存储那些已经收到客户端的 SYN 包 但还没有完成三次握手的连接请求
     全连接队列用于存储那些已经完成三次握手 等待服务器调用 accept 函数处理的连接请求

     2.backlog：指定了允许在队列中等待处理的最大连接请求数量
    */
    acceptSocket_.listen();
    /*
     update() sockfd + 读事件 到主线程下的 EventLoop->poller
     当epoll监听到有数据到来的时候 会触发读回调 也就是上面绑定的 handleRead() 
    */
    acceptChannel_.enableReading(); // 把 读感兴趣 注册到 mainloop 的 poller 当中
}

/*
 有新用户连接的时候 触发handleRead()
*/
void Acceptor::handleRead(){
    InetAddress peerAddr;
    /*
     1.accept 函数时 操作系统会从全连接队列中取出一个已经完成三次握手的连接请求 
     并为该连接创建一个新的套接字描述符 这个新的套接字描述符用于与客户端进行数据传输
     
     2.全连接队列为空 accept 函数 <-> 阻塞/ 非阻塞
    */
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) // 说明此时有连接到来
    {   if (newConnectionCallback_){
        newConnectionCallback_(connfd, peerAddr); // 对 connfd 执行对应的回调 也就是放入 subloop 当中
        }else{
        ::close(connfd); // 没有对应处理逻辑 关掉连接 
        }
    else {
        // 当accept系统调用执行失败时，进入此else分支
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);

        // 检查错误码是否为EMFILE（表示进程已打开的文件描述符数量达到系统限制）
        if (errno == EMFILE) {
         // 用于特别标识因"文件描述符耗尽"这一特定原因导致的accept错误，方便后续问题排查
            LOG_ERROR("%s:%s:%d socket reached limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
    }
}