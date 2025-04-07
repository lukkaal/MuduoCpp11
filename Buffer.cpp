#include "Buffer.h"
#include <sys/uio.h>
#include <error.h>
/*
 从fd上读数据 Poller工作在LT模式
 Buffer缓冲区是有大小的 但是读数据的时候 是不知道tcp数据最终的大小的

 这段代码通过 readv 系统调用高效地从文件描述符
 读取数据
 到 Buffer 对象及其额外的临时缓冲区 extrabuf 中
*/
ssize_t Buffer::readFd(int fd, int* saveErrno){
    char extrabuf[65536] = {0}; // 栈上的内存空间
    /*
     ssize_t readv(int fd, const struct iovec* iov, int iovcnt)

     struct iovec{
        void *iov base; // 缓冲区起始地址
        size_t iov_len // 缓冲区的长度
     }
     iovcnt 表示的是 多个 iov 说明这个函数可以自动的填充多个缓冲区
    */
    struct iovec vec[2]; // 包含两个 iovec 结构体元素的数组 vec
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *saveErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) { // 足够存储 不需要写到 extrabuf
        writeIndex_ += n;
    } else { // 已经开始写到了extrabuf里面了
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

/*::write向 fd 写数据，从 buffer 读缓存区拿数据 但是不会对 buffer 本身造成影响
  所以写完之后 还需要调用 buffer::retrieve() 对缓冲区进行清理
*/
ssize_t Buffer::writeFd(int fd, int *saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *saveErrno = errno;
    }
    return n;
}