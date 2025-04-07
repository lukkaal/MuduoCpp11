#pragma once
#include "noncopyable.h"
#include <functional>
#include <vector>
#include <atomic>
#include <Timestamp.h>
#include <memory>
#include <mutex>
class Channel;
class Poller;
// 事件循环类 包含了两大模块 Channel Poller(epoll 的抽象)
class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;// 回调函数类型
    EventLoop();
    ~EventLoop();

    // 事件循环
    void loop(); // 事件循环
    void quit(); // 退出事件循环
    Timestamp pollReturnTime() const { return pollReturnTime_; } // 返回 poller返回时间
    
    // 在当前loop执行或唤醒loop所在线程执行cb
    void runInLoop(Functor cb); // 在当前loop线程执行cb
    void queueInLoop(Functor cb); // 将cb放入队列中

    // 唤醒loop所在的线程
    void wakeup(); 

    // 接口 从EventLoop中 调用到Poller中的方法
    void updateChannel(Channel* channel); // 更新channel
    void removeChannel(Channel* channel); // 移除channel
    bool hasChannel(Channel* channel); // 判断channel是否在当前loop中

    // 判断当前loop是否在当前线程中运行
    bool isInLoopThread() const { return threaId_ == CurrentThread::tid(); }



private:
    void handleRead(); // 处理wakeupfd的读事件
    void doPendingFunctors(); // 执行回调函数队列中的回调函数

    using ChannelList = std::vector<Channel*>; // Channel列表
    std::atomic_bool looping_; // 是否处于循环状态 CAS原子操作
    std::atomic_bool quit_; // 是否退出循环

    const pid_t threaId_; // 当前loop所在线程ID
    
    Timestamp pollReturnTime_; // poller 返回发生事件的 channels 的返回时间
    std::unique_ptr<Poller> poller_; // Poller对象

    int wakeupFd_; // 用于唤醒poller的fd -> mainReactor 使用轮训算法用于跨线程唤醒 subReactor
    std::unique_ptr<Channel> wakeupChannel_; // 用于唤醒poller的channel

    ChannelList activeChannels_; // 当前活跃的channel列表
    Channel* currentActiveChannel_; // 当前正在处理的channel

    // std::atomic_bool eventHandling_; // 是否处于处理事件状态
    std::atomic_bool callingPendingFunctors_; // 标志位 是否有需要调用回调函数 
    std::vector<Functor> pendingFunctors_; // 存储Loop需要执行的回调函数列表
    
    
    // 因为pendingFunctors_是在其他线程中调用queueInLoop()函数添加的 所以为了线程安全需要加锁
    std::mutex mutex_; // 互斥锁 用来保护pendingFunctors_的线程安全操作

};
