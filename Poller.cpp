#include "Poller.h"
#include "Channel.h"
Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop) {}

// 在map当中查找channel 找到了并且能够对应上是同一个channel
bool Poller::hasChannel(Channel* channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}