#pragma once 
#include "noncopyable.h"
#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string> 
#include <atomic>

class Thread : noncopyable{
public:
    // 如果需要传递参数 可以使用std::bind()函数 
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc, const std::string& name = std::string()); // 拒绝隐式转换
    ~Thread();

    void start(); // 启动线程
    int join(); // 等待线程结束
    bool started() const { return started_; } // 判断线程是否启动
    pid_t tid() const { return tid_; } // 获取线程ID
    const std::string& name() const { return name_; } // 获取线程名字

    static int numCreated() { return numCreated_; } // 获取已创建的线程数
private:
    void setDefaultName(); // 设置默认线程名字

    bool started_; // 线程是否启动
    bool joined_; // 线程是否加入

    // 线程对象(绑定线程函数会直接启动线程 所以选用智能指针来封装以便自行控制)
    std::shared_ptr<std::thread> thread_; 
    
    // typedef __int64	_pid_t;
    pid_t tid_; // 线程ID
    ThreadFunc func_; // 线程函数
    std::string name_; // 线程名字
    static std::atomic_int numCreated_; // 已创建的线程数

};