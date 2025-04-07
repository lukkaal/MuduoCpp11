#include <Thread.h>
#include <CurrentThread.h>
#include <semaphore>
// 使用直接初始化是为了保证语义清晰 避免不必要的拷贝
std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
: started_(false)
, joined_(false)
, tid_(0)
, func_(std::move(func)) // 移动构造
, name_(name)
{
    setDefaultName();
}

void Thread::setDefaultName(){
    int num = ++numCreated_;
    if (name_.empty()){
        char buf[32];
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}

/*从初始化 Thread 类的主线程中 detach 掉 
 主线程可以继续执行其他任务 不必等待这些处理客户端连接的线程结束
 除非主线程join
*/
Thread::~Thread(){
    if (started_ && !joined_){ // 新启动的线程 但是没有join
        thread_->detach(); // 分离线程
    }
}

/*
 创建一个新线程时 操作系统会异步地启动这个新线程
 使用信号量 sem 我们确保了在 start() 函数返回之前 新线程已经成功启动并获取了其线程 ID
*/
void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    // std::shared_ptr<std::thread> thread_; 异步创建
    thread_ = std::make_shared<std::thread>(new std::thread([this](){
        tid_ = CurrentThread::tid(); // 获取线程ID
        sem_post(&sem);
        func_(); // 执行线程函数
    }));
    sem_wait(&sem);
}

void Thread::join(){
    assert(started_);
    assert(!joined_);
    joined_ = true;
    thread_->join(); // 等待线程结束
}