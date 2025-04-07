#include "Channel.h"
#include <sys/epoll.h> // 底层封装是epoll模型
#include "EventLoop.h"
# include "Logger.h"
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop ChannelList
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false),
    eventHandling_(false),
    addedToLoop_(false) 
      {}
Channel::~Channel() {
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread()) {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj; // 绑定共享资源避免对象提前析构
    tied_ = true; // 标志位
}

// 当改变channel所表示的fd的事件时 调用update()函数 在epoll当中更改fd关心的事件
void Channel::update() {
    addedToLoop_ = true;
    // 通过channel所属的EventLoop对象 来调用epoll当中的方法 ***epoll_ctl***
    loop_->updateChannel(this); // 传入了指针
}

// 在所属的EventLoop对象中移除该Channel
void Channel::remove() {
    assert(isNoneEvent()); // 确保没有关心的事件
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock(); // 将weak_ptr转换为shared_ptr 赋值给guard来保证对象的生命周期
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    LOG_INFO("Channel::handleEvent() fd = %d revents = %d\n", fd_, revents_);
    eventHandling_ = true;
    // 通过revents_来判断fd当前发生的事件
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
    } // 如果发生了 EPOLLHUP（对端关闭连接）事件但没有发生 EPOLLIN（可读事件）事件
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_(); // 如果发生错误
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_(receiveTime);
    } // 查是否发生了 EPOLLIN（可读事件）、EPOLLPRI（有紧急数据可读）或 EPOLLRDHUP（对端关闭连接）事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    } // 查是否发生了 EPOLLOUT（可写事件）事件
    eventHandling_ = false;
}   