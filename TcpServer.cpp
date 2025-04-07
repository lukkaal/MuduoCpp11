#include "TcpServer.h"
#include "Logger.h"
#include <functional>


EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress &listenAddr,
                     Option option = kNoReusePort)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1){
        /*
         新用户连接时 TcpServer::newConnection
         -> 
        */
        acceptor_->setNewConnectionCallback([this](auto arg1, auto arg2) {
            this->newConnection(arg1, arg2);}) // connfd,  peerAddr
    };

TcpServer::~TcpServer() {
    for (auto &item : connections_) {
        /*
         using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr> 原型
         using TcpConnectionPtr = std::shared_ptr<TcpConnection>; 原型
         这个局部的 shared_ptr 智能指针对象 出右括号可以自动释放 new 出来的 TcpConnection 对象资源

         挨个销毁每个连接
        */
        TcpConnectionPtr conn(item.second);
        item.second.reset(); // 释放原来的指针 而使用局部对象来接受现有对象

        // 销毁链接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}


void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start(){
    if (started_++ == 0) // 防止一个Tcpserver对象被start多次
    {
        /*
         TcpServer::start(cb)-> EventLoopThreadPool::start(cb)
         -> EventLoopThread(cb, buf)::callback_(cb)
         -> EventLoopThread::threadFunc(cb)中使用 callback
         当 EventLoopThread 管理的线程 thread_ 启动的时候 调用threadFunc
         会先执行这个callback(如果有的话) 然后打开loop循环
        */
        threadPool_->start(threadinitcallback_);
        /*
         .get()获取acceptor的裸指针
         这里的runInLoop是在本线程当中调用 所以会直接执行这里bind的函数
         如果不是的话 有可能只是别的线程(主线程)拿到了这个EventLoop的指针
         所以直接会放入这个 EventLoop 的 pendingFunctors_(queueInLoop)
         subloop一直在执行事件循环
        */
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}
/*
 这里的 sockfd 指的是 connfd, peer
*/
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr){
    // 轮询算法 选择一个 subLoop 来管理 channel
    EventLoop *ioLoop = threadPool_->getNextLoop();

    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;  // 这个不涉及线程安全问题
 
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s",
             name_.c_str(),
             connName.c_str(), // 服务器本身的端口号
             peerAddr.toIpPort().c_str()); // 客户端的地址号

    // 通过 sockfd 获取其绑定的本机的 ip 地址和端口信息 
    sockaddr_in local;
    ::bzero(&local, sizeof(local));
    socklen_t addrLen = sizeof(local);
    if (::getsockname(sockfd, (sockaddr *)&local, &addrLen) < 0) {
        LOG_ERROR("TcpServer::newConnection - getsockname sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 根据连接成功的 sockfd，创建 TcpConnection 连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    // 下面的回调都是用户设置给 TcpServer => TcpConnection => Channel => Poller => notify channel 调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    /*
     关闭连接 一系列的操作 包括 TcpServer 指定 loop 让 epoll 移除底层监听
     并且在 TcpServer 维护的map当中自行移除掉这个 TcpConnection
     用户会调用 conn->shutDown() 来试图关闭这个 TcpConnection
    */
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    // 直接调用在 loop 循环当中使用了 TcpConnection::connectEstablished 为 channel 建立连接
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));


}

//这里 TcpConnectionPtr 别写错成了 TcpConnection
void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    LOG_INFO("TcpServer::removeConnectionInLoop - name [%s], connection [%s]", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}