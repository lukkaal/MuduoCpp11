#pragma once
#include <vector>
#include <string>
#include <algorithm>
/*
 服务端给客户应用准备的数据的缓冲区位置
*/
class Buffer{
public:
    /*
     prependable(记录大小 8)- readable- writable
     0 - readindex - writeindex - size(1024 + 8)
    */
    static const size_t kCheapPrepend = 8; 
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writeIndex_(kCheapPrepend)
    {}
    size_t readableBytes() const{
        return writeIndex_ - readerIndex_;
    }
    size_t writableBytes() const{
        return buffer_.size() - readerIndex_;
    }

    size_t prependableBytes() const{
        return readerIndex_;
    }
    // 返回缓冲区中可读数据的起始地址
    const char* peek() const{
        return begin() + readerIndex_;
    }

    // 还有一部分没有读完 设置一下可读的位置
    void retrieve(size_t len){
        if (len < readableBytes()){
            readerIndex_ += len;
        }else{
            retrieveAll();
        }
    }
    // 全部取完了 重新设置缓冲区
    void retrieveAll(){
        readerIndex_ = writeIndex_ = kCheapPrepend;
    }
    
    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回
    std::string retrieveAsString(size_t len){
        std::string result(peek(), len); // char* + 长度
        retrieve(len);
        return result;
    }

    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes()); // 可读取数据的长度
    }
    
    // 确认是不是有足够的空间去写数据 否则直接扩容
    void ensureWriteableBytes(size_t len){
        if (writableBytes() < len){
            makeSpace(len);
        }
    }

    // 把[data, date + len] 添加到 writeIndex_ 开始的缓冲区当中
    void append(const char* data, size_t len){
        ensureWriteableBytes(len);
        std::copy(data, data+len, beginWrite());
        writeIndex_ += len;
    }

    
    ssize_t readFd(int fd, int* saveErrno); // 从fd上读取数据
    ssize_t writeFd(int fd, int *saveErrno);  // 通过 fd 发送数据

    char* beginWrite(){
        return (begin() + writeIndex_);
    }

    const char* beginWrite() const{ 
        return (begin() + writeIndex_);
    }
private:
void makeSpace(size_t len)
{
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
        buffer_.resize(writeIndex_ + len);
    }
    else
    {
        size_t readable = readableBytes();
        std::copy(begin() + readerIndex_,
                  begin() + writeIndex_,
                  begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writeIndex_ = readerIndex_ + readable;
    }
}
    char* begin(){
        /*
         iterator.operator*() 调用了*的重载函数获得了首元素的值
         加上& 获取了数组首元素的地址
        */
        return &*buffer_.begin();
    }
    const char* begin() const{
        /*
         iterator.operator*() 调用了*的重载函数获得了首元素的值
         加上& 获取了数组首元素的地址
        */
        return &*buffer_.begin();
    }
    


    std::vector<char> buffer_; // 缓冲区 8+1024 的总大小
    size_t readerIndex_;
    size_t writeIndex_;
};