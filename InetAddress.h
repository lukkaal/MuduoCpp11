#pragma once 
#include <netinet/in.h>
#include <string>
// 封装socket地址类型
//sockaddr_in 结构体主要用于在网络编程中表示和操作 IPv4 地址信息
class InetAddress {
public:
    explicit InetAddress(uint16_t port, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr) {}
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;
    const sockaddr_in* getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in &addr){addr_ = addr;}
private:
    sockaddr_in addr_;
};

// struct sockaddr_in {
//     short            sin_family;   // 地址族，一般为 AF_INET 表示 IPv4
//     unsigned short   sin_port;     // 端口号，使用网络字节序
//     struct in_addr   sin_addr;     // IPv4 地址结构体
//     char             sin_zero[8];  // 填充字节，使 sockaddr_in 和 sockaddr 长度相同
// };

// struct in_addr {
//     unsigned long s_addr;  // 32 位的 IPv4 地址，使用网络字节序
// };