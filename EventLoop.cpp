#include "EventLoop.h"
#include "Channel.h"
#include <sys/eventfd.h> // eventfd
#include <unistd.h> // read write close
#include <fcntl.h>
#include "Logger.h"
#include <error.h>
// 防止一个线程创建多个 EventLoop
__thread EventLoop* t_loopInThisThread = nullptr; // 线程局部存储

// Poller 默认的超时时间
const int kPollTimeMs = 10000;

// 创建一个 eventfd 文件描述符 用来唤醒subReactor 处理新来的channel
int createEventfd(){
    // 创建一个 eventfd 文件描述符
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0){
        LOG_FATAL("Failed in eventfd");
    }
    return evtfd;
}

EventLoop::EventLoop()
    :looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_(false)
    ,threaId_(CurrentThread::tid()) // 调用 封装好的 CurrentThread::tid() 获取当前线程的ID
    ,poller_(Poller::newDefaultPoller(this)) // 判断环境变量是linux或其他 创建一个默认的Poller
    ,wakeupFd_(createEventfd()) // 创建一个 eventfd 文件描述符
    // ,wakeupChannel_(new Channel(this, wakeupFd)) // 创建一个channel 用于唤醒poller
    ,wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_))
    ,currentActiveChannel_(nullptr) // 当前活跃的channel
{LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId);
    // 如果当前线程已经创建了EventLoop对象
    if (t_loopInThisThread){
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId);
    } else {
        t_loopInThisThread = this;
    }
    
    // 此处封装 Channel 的逻辑 设置回调函数 并且将 Channel 加入到poller中
    // & 用于获取成员函数的地址
    wakeupChannel->setReadCallback(std::bind(&EventLoop::handleRead, this));// 设置唤醒事件的回调函数 
    wakeupChannel->enableReading(); // 注册读事件到Poller 监听wakeupFd的EPOLLIN事件

}


// wakeupFd 被写入数据会让 eventfd 内部的计数器值增加 同时触发 wakeupFd 的可读事件 将其添加到EventList中
// 读取以清空 eventfd 内部的计数器 避免该事件持续触发
void EventLoop::handleRead(){
    uint64_t one = 1;
    // read 函数尝试从 wakeupFd 中读取 sizeof one（即 8 字节）的数据到 one 变量所在的内存地址
    ssize_t n = read(wakeupFd, &one, sizeof one);
    // 如果实际读取的字节数不等于 8 字节，说明读取过程可能出现了错误
    if (n != sizeof one){
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
    // 读取操作完成后，通常意味着 wakeupFd 上的可读事件已被处理
}

EventLoop::~EventLoop(){
    wakeupChannel->disableAll(); // 关闭所有事件
    wakeupChannel->remove(); // 从 Poller 中移除
    ::close(wakeupFd); // 关闭文件描述符
    t_loopInThisThread = nullptr; // 线程局部存储置空
}
 
void EventLoop::loop(){
    assert(!loop);
    assertInLoopThread(); // 断言当前线程是在创建该对象的线程
    loop = true;
    quit = false;
    LOG_INFO("EventLoop %p start looping\n", this);
    while (!quit_){
        // EpollPoller::poll() 会根据epoll_wait()返回对应的发生的事件到activeChannels中
        activeChannels_.clear(); // 每次循环开始时 清空活跃的通道
        pollReturnTime_ = poller_.poll(kPollTimeMs, &activeChannels_); // 初始化时创建的Poller对象
        for (Channel* channel: activeChannels_){
            currentActiveChannel_ = channel;
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }

        // 除了自己 epoll_wait 到的回调之外 还有来自其他线程添加到的回调
        doPendingFunctors(); // 执行回调函数队列中的回调函数
    }
} 

/*
doPendingFunctors()
EventLoop 线程通常负责处理 I/O 事件（像网络套接字的读写、文件描述符的状态变化等）
而其他线程可能需要向 EventLoop 线程发送一些额外的任务 比如更新 UI、执行定时任务等
wakeup 本身只是文件描述符 handleRead也只是读取数据清空计数器
而实际真正的任务是通过 queueInLoop() 函数添加到 pendingFunctors_ 队列中的
由doPendingFunctors()函数执行<-----
*/

// 可能是其他线程调用的 quit()
void EventLoop::quit(){
    quit_ = true;
    if (!isInLoopThread()){
        wakeup(); // 如果不在当前loop线程中 唤醒运行事件循环的线程(wakeupfd & wakeupchannel)(也就是执行.loop()的线程)
    }
}

// 在当前loop执行或唤醒loop所在线程执行cb
void EventLoop::runInLoop(Functor cb){
    if (isInLoopThread()){
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_); // 加锁
        pendingFunctors_.emplace_back(std::move(cb)); // 添加到队列中
    }
    // 如果不在当前loop线程中 唤醒运行事件循环的线程(wakeupfd & wakeupchannel)
    /*
    callingPendingFunctors_ 标志位可以确保在执行当前一轮的回调函数时
    新添加的回调函数会被推迟到下一轮 EventLoop 循环中执行
    因为有可能此时正在执行 doPendingFunctors() 函数
    */
    if (!isInLoopThread() || callingPendingFunctors_){
        wakeup(); // 唤醒
    }
}

// 向wakeupfd写一个数据来唤醒poller
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one); // 来自io.h 的系统调用 会触发wakeupChannel的可读事件
    // 如果实际写入的字节数不等于 8 字节，说明写入过程可能出现了错误
    if (n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %ld bytes instead of 8", n);
    }
}

void EventLoop::doPendingFunctors(){
    /* 如果说直接占用了pendingFunctors_的话 因为需要加锁
      会导致其他线程在使用 runInLoop() -> queueInLoop() 时 
      需要一直阻塞等待 直到获取到锁
      对于主线程来说是不可接受的 因为主线程需要不断的处理新的连接
    */
    std::vector<Functor> functors; // 使用拷贝的方法
    callingPendingFunctors_ = true; // 正在调用doPendingFunctors()函数
    {
        // 只需要在复制的时候才加锁 不需要在执行的时候加锁
        std::unique_lock<std::mutex> lock(mutex_); // 加锁
        functors.swap(pendingFunctors_); // 交换两个vector的内容
    }
    for (const Functor& functor : functors){
        functor(); // 执行回调函数 functor = std::function<void()> -> 加括号执行 functor()
    }
}


void EventLoop::updateChannel(Channel* channel){
    assert (channel->ownerLoop() == this);
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel){
    assert (channel->ownerLoop() == this);
    poller_->removeChannel(channel);
}

