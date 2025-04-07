#include "CurrentThread.h"
#include <sys/syscall.h>
namespace CurrentThread {
    __thread int t_cachedTid = 0; // 每个线程都有自己独立的副本
    __thread char t_tidString[32];
    void cacheTid(){
        if (t_cachedTid == 0){
            // 通过linux系统调用 获取当前线程的tid
            t_cachedTid = static_cast<int>(::syscall(SYS_gettid));
        }
    }
}