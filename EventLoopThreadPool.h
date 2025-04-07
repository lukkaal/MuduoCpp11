#pragma once 
#include "noncopyable.h"
#include "EventLoop.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads){numThreads = numThreads;}

    void start(const ThreadInitCallback& cb = ThreadInitCallback());
    // 如果是在多线程当中 baseloop_ 默认会以轮询的方式分配channel给 subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const {return started;}
    const std::string name() const {return name_;};

private:
    EventLoop* baseloop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    /*
    通过调用 EventLoopthread -> startLoop()
    两个 vector 之间建立 线程-EventLoop对象 之间的联系
    one loop per thread
    */
    // 线程池 vector
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    // 通过使用 EventLoop* EventLoopThread::startLoop() 获取到 EventLoop 的指针
    std::vector<EventLoop*> loops_;
};