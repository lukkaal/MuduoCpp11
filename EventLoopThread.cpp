#include "EventLoopThread.h"
#include "EventLoop.h" // Include the header file for EventLoop
#include <memory>
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
    :loop_(nullptr)
    ,exiting_(false)
    ,thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    ,mutex_()
    ,cond_()
    ,callback_(cb)
{}

EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();// 一开始线程启动的时候 detach 过 这里需要释放资源所以 join
    }
}

EventLoop* EventLoopThread::startLoop(){
    /*
    Thread类是重新封装过的 在EventLoopThread类中初始化时也初始化了Thread类
    传入了需要执行的线程函数threadFunc 存入了 Thread->func 成员变量
    执行.start()函数时 会新建线程并在线程中执行func_()函数
    */
   /*
    但是启动之后 线程初始化并且执行threadFunc()
    需要一定的时间才能将 loop_ 成员变量初始化成新建的 EventLoop
    所以使用条件变量 等待 loop_ 初始化
    确保start()生命周期内 该 EventLoopThread 获取 EventLoop
   */
    thread_.start();
    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop == nullptr){
            cond_.wait(lock); // 线程间通信
        }
        loop = loop_;
        return loop;
    }

}

/*
 EventLoopThread 成员函数 提前定义
 用于新建一个EventLoop 并赋值给成员变量 loop_
*/
void EventLoopThread::threadFunc(){
    EventLoop loop;
    if (callback_){
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock();
        loop_ = &loop; // 将成员变量指向新建的EventLoop对象
        cond_.notify_one();
    }
    // 执行事件循环
    loop.loop();
    // 执行到这里 说明已经退出了循环 需要释放掉 loop_ 
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
