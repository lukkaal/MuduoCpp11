#pragma once
#include "Poller.h"
#include <vector>
#include <sys/epoll.h>
#include <map>
class Channel;
class EventLoop;
class EpollPoller : public Poller {
/*
epoll:
epoll_create
epoll_ctl
epoll_wait
*/

public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    // 重写Poller的纯虚函数(抽象方法)
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
private:
    static const int kInitEventListSize = 16; // 初始化事件列表大小size
    // static const char* operationToString(int op); // 将操作转换为字符串

    // 用于填写活跃的连接通道 和 更新通道
    void fillActiveChannels(int numEvents , ChannelList* activeChannels) const; // 填充活动通道
    void update(int operation, Channel* channel); // 更新通道

    // 用于存放 epoll_wait 返回的就绪事件
    using EventList = std::vector<epoll_event>; // 用于存放 epoll_wait 返回的就绪事件
    int epollfd_;// epoll_create 返回的文件描述符 表示一个 epoll 实例
    EventList events_; // 用于存放 epoll_wait 返回的就绪事件

     
};


// typedef union epoll_data {
//     void        *ptr;
//     int          fd;
//     uint32_t     u32;
//     uint64_t     u64;
// } epoll_data_t;

// struct epoll_event {
//     uint32_t     events;      //EPOLLIN etc./* Epoll events */
//     epoll_data_t data;        //fd /* User data variable */
// };