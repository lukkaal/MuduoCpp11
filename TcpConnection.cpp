#include "TcpConnection.h"

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

#include <errno.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL("TcpConnection [static]CheckLoopNotNull - Loop is null!");
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd)) // 轮询到的 loop 和 ::accept 返回得到的 connfd
    , localAddr_(localAddr)
    , peerAddr_(peerAddr) // 通过 Socket::accept 函数获取到
    , highWaterMark_(64 * 1024 * 1024) {
    //!NOTE: 和 acceptChannel 区分开，那个是 listenfd 只关心 setReadCallback，这个 channel 是 connfd 需要关心读写关闭以及错误
    // 下面给 channel_ 设置相应的回调函数，poller 给 channel 通知感兴趣的事件发生了，channel 会回调相应的操作函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleClose, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d", name_.c_str(), (int)state_);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d", name_.c_str(), channel_->fd(), (int)state_);
}

// 发送数据
void TcpConnection::send(std::string &buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        } else {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

/*
 应用写的快而内核发送数据慢 所以需要把发送数据写入缓冲区 
 而且设置了水位回调
*/
void TcpConnection::sendInLoop(const void *data, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 之前调用过该 connection 的 shutdown 不能再进行发送了
    if (state_ == kDisconnected) {
        LOG_ERROR("TcpConnection::sendInLoop - disconnected, give up writing!");
        return;
    }

    /*
     表示 channel 第一次开始写数据 而且缓冲区没有发送数据
     所以尝试直接将这个数据写入到 fd
     send()中调用的时候 传入string 这里尝试将string直接写入 fd
    */
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                /* 
                 既然这里数据全部发送完成，就不用再给 channel 设置 epollout 事件
                 所以直接调用 写完成回调函数
                */
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else {  // nwrote < 0
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop - errno = %d", errno);
                if (errno == EPIPE || errno == ECONNRESET)  // SIGPIPE | RESET
                {
                    faultError = true;
                }
            }
        }
    }

    /*
     说明当前这一次 write 并没有把数据全部发送出去 剩余的数据需要保存到缓冲区中
     然后这里手动给 channel 注册 epollout 事件 让 ***递归调用*** 这个 handleWrite 函数
     也就是调用 TcpConnection::handleWrite 方法 把发送缓冲区的数据全部发送完成
     */
    if (!faultError && remaining > 0) {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        /*
         如果现在要发送的长度 + 现在缓冲区outputBuffer_ 里有的长度 大于 水位max
         && 在添加剩余数据之前，缓冲区的数据量还未达到水位线
         && 水位回调函数存在
         将回调函数丢进 pendingFunctor 队列 在不断地 Loop 当中执行
        */
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);

        // 注册 channel 的写事件 否则 poller 不会给 channel 通知 epollout
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

// 关闭写连接 使得端口还可以继续读
void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// shutdown 过程 channel_ 还没有写完 直到 readableBytes() == 0 和 handleWrite() 关联
void TcpConnection::shutdownInLoop() {
    if (!channel_->isWriting()) // 说明 outputBuffer 中的数据已经全部发送完成
    { 
        /*
         只要对端继续发送数据 epoll 会检测到 EPOLLIN 事件 从而触发相应的读操作
         关闭写端 EPOLLHUP 自动注册当对端也关闭了连接（发送了 FIN 包）本地会接收到这个 FIN 包
         epoll 会检测到 EPOLLRDHUP 或 EPOLLHUP 事件 从而触发相应的关闭连接操作
        */
        socket_->shutdownWrite();
    }
}

// 连接建立，当 TcpServer 接受到一个新连接时被调用
void TcpConnection::connectEstablished() {
    setState(kConnected);
    /*
     防止上层将 TcpConnection 给 remove 掉而 callback 执行出错
     调用 tie 方法 将 this 打包成一个 shared 智能指针 让 channel 引用保证其生命周期
     当 channel 内部执行 handleEvent 的时候 首先就会判断 if (tied_)
     然后才会执行相应的 handleEventWithGuard (即根据 revent 的 类型执行相关回调)
    */
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向 poller 注册 channel 的 epollin 事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁，当 TcpServer 移除时连接时被调用
void TcpConnection::connectDestroyed() {
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();  // 把 channel 所有感兴趣的事件，从 poller 中 del 掉
        connectionCallback_(shared_from_this());
    }

    channel_->remove();  // 把 channel 从 poller 中删除掉
}

// 从 connfd 读取数据到 inputBuffer_ 并执行上层设置的 messageCallback_
void TcpConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    /*
     已经建立连接的用户 有可读事件发生了 调用读回调
     shared_from_this() 返回当前对象的 shared_ptr
     readFd 将 fd 获取到的数据读到 inputBuffer_ 的可写区
     然后执行 messageCallback_ 对读到的数据进行处理
    */
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) { // 断开连接 
        handleClose();
    } else {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead - errno = %d", errno);
        handleError();
    }
}

/*
 connfd 的写回调函数执行 将 outputBuffer_ 上的可读数据向 connfd 写
 包括维护 outputBuffer 本身的 Index 等
 并执行上层设置的 writeCompleteCallback_
*/
void TcpConnection::handleWrite() {
    /*
     为什么要判断 channel_->isWriting()？
     因为注册写事件通常是有条件的 注册之后 内核发送缓冲区有空间的时候
     就会触发 fd 的写事件 这时候就会将 outputBuffer_中的数据发到 内核缓冲区
     当 outputBuffer_ 没有数据可发的时候 outputBuffer_.readableBytes() == 0
     就需要写事件的继续触发
    */
    if (channel_->isWriting()) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0) {
            outputBuffer_.retrieve(n);
            /*
             执行里面逻辑的时候 说明 outputBuffer_ 中数据已经发送完成
            */
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();  // 写完了变成不可写
                /*
                 实际上就是本线程调用的
                 只是并不紧急所以放入到 pendingFunctors 当中  
                */ 
                if (writeCompleteCallback_) {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }

                /*
                 数据没发送完会handleWrite
                 数据发送完了 还是会调用到shutdownInLoop
                */
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite() - errno = %d", errno);
        }
    } else {
        LOG_ERROR("TcpConnection::handleWrite() - fd = %d is done, no more writing", channel_->fd());
    }
}

// poller => channel::closeCallback => TcpConnection::handleClose
void TcpConnection::handleClose() {
    LOG_INFO("TcpConnection::handleClose() - fd = %d, state = %d", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    //这里再次调用 connectionCallback_ 处理断开事件的 callback，实际上是给用户一个提示 disConnected，没有处理
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);  // 关闭连接的回调 => 执行的是 TcpServer::removeConnection 回调方法
}

void TcpConnection::handleError() {
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        err = errno;
    } else {
        err = optval;
    }

    LOG_ERROR("TcpConnection::handleError() - name: %s, SO_ERROR: %d", name_.c_str(), err);
}