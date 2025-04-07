#pragma once

// 防止拷贝的基类 比如TcpServer类不希望被拷贝就需要继承这个基类
class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};