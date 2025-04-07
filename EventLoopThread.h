#pragma once
#include "noncopyable.h"
#include <functional>
#include "Thread.h"
#include <mutex>
#include <condition_variable>
#include <string>
class EventLoop;

class EventLoopThread : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cd = ThreadInitCallback(),
                    const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop(); // 开启循环   
private:
    void threadFunc(); // 线程函数 用来创建EventLoop对象
    EventLoop* loop_;
    bool exiting_; // 是否退出
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;

};