#pragma once
#include <unistd.h>
#include <sys/syscall.h>
namespace CurrentThread{
    // 声明 t_cachedTid 变量是为了在多个源文件之间共享这个变量的声明
    extern __thread int t_cachedTid;
    void cacheTid();
    inline int tid(){
        //  __builtin_expect(long exp, long c) 是 GCC (version > 2.96) 的内建函数
        // 作用是将 exp 的值为真的可能性告诉编译器，以便编译器对代码进行优化
        if (__builtin_expect(t_cachedTid == 0, 0)){
            cacheTid();
        }
        return t_cachedTid;
    }
}


/*
一般在使用的时候 CurrentThread::tid() 这样调用
*/