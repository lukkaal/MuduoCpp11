#include "Logger.h"
#include <iostream>
#include "Timestamp.h"
Logger& Logger::GetInstance() {
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level) {
    LogLevel_ = level;
}

// 写日志格式遵循：[level] time: msg
void Logger::log(std::string msg) {
    switch (LogLevel_)
    {
    case INFO:
        std::cout << "[INFO] ";
        break;
    case ERROR:
        std::cout << "[ERROR] ";
        break;
    case FATAL:
        std::cout << "[FATAL] ";
        break;
    case DEBUG:
        std::cout << "[DEBUG] ";
        break;
    default:
        break;
    }

    std::cout << Timestamp::now().toString() << msg << std::endl;

}