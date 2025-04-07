#include "Poller.h"
#include <stdlib.h>
#include "EPollPoller.h"
Poller* Poller::newDefaultPoller(EventLoop* loop) {
    if (::getenv("MUDUO_USE_POLL")) {
        // 如果设置了 MUDUO_USE_POLL 环境变量，返回基于 poll 机制的 Poller 实例
        // return new PollPoller(loop);
    } else {
        // 否则返回默认的 Poller 实例，通常是基于 epoll 机制的
        return new EPollPoller(loop);
    }
}