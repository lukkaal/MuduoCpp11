#pragma once
#include "noncopyable.h"
#include <vector>
#include <map>
#include "Timestamp.h"
class Channel;
class EventLoop;
// muduo库当中 多路事件分发器的核心IO复用模块
// 负责监听多个文件描述符上的事件是否就绪
class Poller : noncopyable{
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop* loop);
    // 基类 需要派生类自行实现(这里是抽象层)
    virtual ~Poller(); 
    
    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0; // IO复用的核心 poller抽象接口
    // 在Poller中添加或者更新Channel(在Channel当中是传入的指针类型 所以这里也是指针)
    virtual void updateChannel(Channel* channel) = 0; // 更新通道
    virtual void removeChannel(Channel* channel) = 0; // 移除通道
    // 判断channel是否在当前Poller当中
    bool hasChannel(Channel* channel) const;
    
    // EventLoop可以通过该接口获取默认的IO复用的具体实现***接口***
    static Poller* newDefaultPoller(EventLoop* loop); // 创建一个默认的Poller
protected:
    // fd : Channel* 的映射 
    using ChannelMap = std::map<int, Channel*>; // fd -> Channel*
    ChannelMap channels_; // Poller所管理的Channel实例对象
private:
    EventLoop* ownerLoop_; // Poller所属的EventLoop
    
};