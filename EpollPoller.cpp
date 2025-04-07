#include "EpollPoller.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <errno.h> 
#include <unistd.h>
#include <assert.h>
#include <string.h>
const int kNew = -1; // Channel 的 index_ 初始值 表示未添加到 epoll 中
const int kAdded = 1; // Channel 的 index_ 表示已经添加到 epoll 中
const int kDeleted = 2; // Channel 的 index_ 表示已经从 epoll 中删除

EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)), 
      events_(kInitEventListSize) {
    if (epollfd_ < 0) { // epoll_create1 出错
        LOG_FATAL("epoll_create error:%d\n", errno);
    }
}

EpollPoller::~EpollPoller() {
    ::close(epollfd_);// close是系统调用 关闭文件描述符
}

/*
            EventLoop
    ChannelList    Poller(epoll)
                    ChannelMap

*/

// Channel 的 update()-> EventLoop 的 updateChannel()-> Poller 的 updateChannel -> EpollPoller 的 updateChannel()
// 是两个模块之间的接口 通过epoll_ctl()函数注册的同时 也会更新Channel的index_
EpollPoller::updateChannel(Channel* channel){
    const int index = channel->index();// 获取channel的index_状态
    LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew | index == kDeleted){// 也就是现在没有注册到epoll_ctl去监听
        if (index == kNew){
            int fd = channel->fd();
            channels_[fd] = channel; // 将channel添加到map当中
        }
        channel->set_index(kAdded); // 设置index_为kAdded
        // void update(int operation, Channel* channel)
        update(EPOLL_CTL_ADD, channel); // EpollPoller 自己封装的update 调用epoll_ctl()函数注册 
    }else{ // 已经注册到epoll_ctl去监听
        int fd = channel->fd();
        if (channel->isNoneEvent()){
            update(EPOLL_CTL_DEL, channel); // 从epoll中删除
            channel->set_index(kDeleted); // 设置index_为kDeleted
        }else { // 以前注册过并且还有关心的事件 说明只是关心的事件变了
            update(EPOLL_CTL_MOD, channel); // 修改而不是添加
        }
    }
}

// 调用epoll_ctl()注册 Channel*用来获得fd和相关的event (使用event.data.ptr指向Channel, 对于events使用Channel*来初始化)
void EpollPoller::update(int operation, Channel* channel){
    epoll_event event;
    bzero(&event, sizeof event);// 将其所有成员的值都置为 0
    int fd = channel->fd(); 
    event.events = channel->e   vents(); // 设置events
    event.data.ptr = channel; // 设置data.ptr指向channel
    
    // operation->EPOLL_CTL_MOD etc.
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if (operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl op=%d fd=%d\n", operation, fd);
        }else{
            LOG_FATAL("epoll_ctl op=%d fd=%d\n", operation, fd);
        }
    }
}

void EpollPoller::removeChannel(Channel* channel){
    // __FUNCTION__代表了当前函数的名字 即removeChannel 
    LOG_INFO("func=%s => fd=%d ", __FUNCTION__, channel->fd());
    int index = channel->index();
    assert(index == kAdded || index == kDeleted);
    int fd = channel->fd();
    channels_.erase(channel->fd());
    
    if (index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// epoll_wait()返回活跃的事件列表 timeoutMs是超时时间(-1 0 >0)
Timestamp EpollPoller::poll(int timeoutMs, ChannelList* activeChannels){
    LOG_INFO("func=%s => fd_total_numbers=%d\n", __FUNCTION__, channels_.count());
    // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout); 
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno; // errno是一个全局变量 检查 errno 来确定具体的错误类型   
    Timestamp now(Timestamp::now()); // 获取当前时间
    if (numEvents > 0){
        LOG_INFO("%d events happened\n", numEvents);
        // void fillActiveChannels(int numEvents , ChannelList* activeChannels) const;
        // 将 epoll_wait 返回的 epoll_event 填充到 activeChannels 中
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()){
            // events_本身作为 EpollPoller存储 epoll_wait 返回就绪事件的vector 
            events_.resize(events_.size() * 2);// 扩容 说明可能不够用
        }
    }else if (numEvents == 0){
        LOG_DEBUG("%s => TIMEOUT\n", __FUNCTION__); // 说明超时
    }else{ // numEvents < 0 说明出错
        if (saveErrno != EINTR){ // errno 为 EINTR 表示系统调用被信号中断
            errno = saveErrno;
            LOG_ERROR("EpollPoller::poll() err!\n");
        }
     }
     return now;
}

// 将 epoll_wait 返回的 epoll_event 填充到 activeChannels 中 EventLoop 就从 EpollPoller 拿到了这些 Channel
void EpollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const{
    assert (static_cast<size_t>(numEvents) <= events_.size()); // 不论实际响应的事件有多少 最多只放入events_的大小个
    for (int i = 0; i < numEvents; ++i){
        Channel* channel = static_cast(events_[i].data.ptr);
#ifdef __DEBUG__
        int fd = channel->fd();
        ChannelMap::const_iterator it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
#endif
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}