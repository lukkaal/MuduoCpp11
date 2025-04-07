#include "EventLoopThreadPool.h"
#include "EventLoopThread.h" 

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseloop_(baseLoop)// 用户创建
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0){}

EventLoopThreadPool::~EventLoopThreadPool(){
}


/*
 EventLoopThread 的意义在于封装 Thread 的同时管理这个用来 loop 的新线程
 "管理" = 线程开启可控 + 对新线程的生命周期进行管理(析构时利用 quit() 和 join()实现)
 
 而 EventLoopThreadPool 则是用来封装 EventLoopThread 
 从而实现串联 对各子线程(subloop) 进行生命周期的统一管理
*/
void EventLoopThreadPool::start(const ThreadInitCallback& cb){
    started_ = true;
    for (int i = 0; i < numThreads_ ; ++i){
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        /*
         新创建的EventLoop cb是作为初始化的回调函数
         在创建新线程并开启事件循环之前执行
        */
        EventLoopThread* t = new EventLoopThread(cb, buf);
        // 使用智能指针来管理 new (堆上)出来的内存 放入线程池的记录vector
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        /*
         新开辟的 EventLoopThread 开启线程后返回 EventLoop* 的事件循环类指针
         放入vector进行管理

         EventLoopThreadPool 管理 线程+EventLoop对象
        */
        loops_.push_back(t->startLoop());
    }
     // 整个服务端只有一个线程 用户创建的mainloop(也就是传进来的*EventLoop)
    if (numThreads_ == 0 && cb){
         cb(baseloop_);
    }
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){
    if (loops_.empty()){
        return std::vector<EventLoop*>(1, baseloop_);
    }else{
        return loops_;
    }
}

// 轮询的方式获取 EventLoop
EventLoop* EventLoopThreadPool::getNextLoop(){
    EventLoop* loop = baseloop_;
    if (!loops_.empty()){
        loop = loops_[next_];
        ++next_;
        if (next_ > loops_.size()){
            next_ = 0;
        }
    }
    return loop;
}
