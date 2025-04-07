#pragma once
#include <iostream>
#include "noncopyable.h"
#include <functional>
#include "Timestamp.h"
#include <memory>
class EventLoop;
// 理解为通道 封装了文件描述符fd 和 事件类型event 如EPOLLIN EPOLLOUT事件
// 还绑定了Poller返回的具体事件 
class Channel : noncopyable {
public:
    // 使用using 是增强可读性的方法
    using EventCallback = std::function<void()>;  // 事件回调函数类型 
    using ReadEventCallback = std::function<void(Timestamp)>; // 读事件回调函数类型

    Channel(EventLoop* Loop, int fd);
    ~Channel();

    // 调用相应的回调方法
    void handleEvent(Timestamp receiveTime); // 处理 Channel 所关联的文件描述符上发生的事件
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb);}

    // tie() 函数的作用是将 Channel 和某个对象绑定在一起 
    // 防止对象提前析构
    void tie(const std::shared_ptr<void>&);
    int fd() const { return fd_; } // 返回文件描述符
    int events() const { return events_; } // 返回关心的事件

    // epoll EventLoop 和 channel 之间的接口
    void set_revents(int revt) { revents_ = revt; } // epoll监听到fd发生的事件之后 EventLoop设置fd对应channel活动的事件
    

    // update() 的更新实际上就是调用 EventLoop 的 updateChannel() 函数 
    // (本质是epoll_ctl)
    void enableReading() { events_ |= kReadEvent; update(); } // 开启读事件
    void disableReading() { events_ &= ~kReadEvent; update(); } // 关闭读事件
    void enableWriting() { events_ |= kWriteEvent; update(); } // 开启写事件
    void disableWriting() { events_ &= ~kWriteEvent; update(); } // 关闭写事件
    void disableAll() { events_ = kNoneEvent; update(); } // 关闭所有事件

    // 返回fd当前关心的事件状态
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }
    bool isNoneEvent() const { return events_ == kNoneEvent; } // 判断是否没有关心的事件

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // for Poller
    EventLoop* ownerLoop() { return loop_; } // 返回所属的 EventLoop 对象的指针
    void remove(); // 从 EventLoop 中移除该 Channel
private:

    void update(); // 更新 Channel 关心的事件
    void handleEventWithGuard(Timestamp receiveTime); // 处理 Channel 所关联的文件描述符上发生的事件
    // 感兴趣的事件状态的描述
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_; // 所属的 EventLoop 对象的指针 用于与 EventLoop 进行交互
    const int fd_; // 文件描述符
    int events_; // 关心的事件
    int revents_; // Poller 通知的目前活动的事件
    int index_; // Poller 内部标识该 Channel 的索引 表示该 Channel 在 Poller 中的状态
    std::weak_ptr<void> tie_; // 弱引用智能指针，用于绑定共享资源
    bool tied_; // 是否已经绑定了某个对象 TRUE 表示已经绑定了某个 TcpConnection
    bool eventHandling_; // 当前是否正在处理事件
    bool addedToLoop_; // 该 Channel 是否已经被添加到 EventLoop 中

    // 因为channel当中可以知道fd正在发生的事件 所以负责调用对应的回调函数
    // 由用户来指定的 channel负责调用
    ReadEventCallback readCallback_; // 读事件回调函数
    EventCallback writeCallback_; // 写事件回调函数
    EventCallback errorCallback_; // 错误事件回调函数
    EventCallback closeCallback_; // 关闭事件回调函数
};