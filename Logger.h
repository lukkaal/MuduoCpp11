#pragma once
#include "noncopyable.h"
#include <string>
// 定义日志的级别 INFO ERROR FATAL DEBUG
enum LogLevel {
    INFO, ERROR, FATAL, DEBUG
};

#define LOG_INFO(logmsgFormat,...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_ERROR(logmsgFormat,...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_FATAL(logmsgFormat,...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)
#ifdef MUDEBUG // 比如在编译选项中添加了-DMUDEBUG 或者在代码前面手动写了#define MUDEBUG 说明此时是调试模式
#define LOG_DEBUG(logmsgFormat,...) \
    do { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)
#else
#define LOG_DEBUG(logmsgFormat,...)
#endif

// 输出一个日志类 单例模式
class Logger : noncopyable {
public:  
    // 获取唯一的实例
    static Logger& GetInstance();
    // 设置日志级别
    void setLogLevel(int level);
    // 输出日志
    void log(std::string msg);

private:
    Logger(){};
    // 日志级别
    int LogLevel_;
    
};

// 调用方式：
// int main() {
//     int num = 10;
//     const char* str = "example";
//     // 调用LOG_INFO宏
//     LOG_INFO("The number is %d and the string is %s", num, str); 
//     return 0;
// }