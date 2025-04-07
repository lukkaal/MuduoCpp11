#include "Timestamp.h"
#include <sys/time.h>
Timestamp::Timestamp(): microSecondsSinceEpoch_(0) {}
explicit Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch) 
    {}

// 获取当前时间并构建一个Timestamp对象
Timestamp Timestamp::now(){
     return Timestamp(time(NULL));// 获取当前时间
}
// 使用now返回对象调用.toString()方法
std::string Timestamp::toString() const {
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    // snprintf是一个标准 C 库函数 用于将格式化的数据写入到指定的字符数组中
    snprintf(buf, 128, "%4d-%02d-%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday,
             tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);
    return buf;
}