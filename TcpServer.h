#pragma once
/*
 用户使用muduo来编写服务器程序
 调用封装化的网络库
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include <functional>
#include <memory>
#include <string>
#include "Callbacks.h"
#include <atomic>
#include <unordered_map>
// 对外服务器编程使用的类
class TcpServer : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option{
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop*, const InetAddress& listenAddr, 
        const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();

    /*
     设置使用到的回调函数
    */
    void setThreadInitcallback(const ThreadInitCallback& cb){
        threadinitcallback_ = cb;
    }
    void setMessageCallback(const MessageCallback& cb){
        messagecallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback& cb){
        connectioncallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){
        writecompletecallback_ = cb;
    }

    void setThreadNum(int numThread); // 设置底层subloop的个数


    /*
     开启服务器的监听
    */ 
    void start(); 
private:
    void newConnection(int sockfd, const InetAddress& peerAddr);
    /*
     using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
     ConnectionMap 用来管理连接 remove用来移除
    */
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;// 用户定义baseloop 传进来

    const std::string ipPort_; // 服务器ip地址端口号
    const std::string name_; // 服务器名称

    std::unique_ptr<Acceptor> acceptor_; // 运行在mainlopo 用来监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectioncallback_; // 有新连接时候的回调
    
    MessageCallback messagecallback_; // 有读写消息时候的回调
    WriteCompleteCallback writecompletecallback_; // 消息发送完成时候的回调

    ThreadInitCallback threadinitcallback_; // loop线程初始化之后的回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_ // 保存 Tcpserver 的所有的连接

}; 