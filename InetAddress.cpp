#include "InetAddress.h"
#include <string>
InetAddress::InetAddress(uint16_t port, std::string ip) {
    addr_.sin_family = AF_INET; // IPv4
    addr_.sin_port = htons(port); // 网络字节序端口号
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); // 网络字节序（大端序）存储的ip地址
}

// 将 InetAddress 对象内部存储的二进制 IPv4 地址转换为点分十进制的字符串形式并返回
std::string InetAddress::toIp() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf)); // 转换后的字符串存放在buf中
    return buf;
}

// 将 InetAddress 对象内部存储的二进制 IPv4 地址和端口号转换为点分十进制的字符串形式并返回
std::string InetAddress::toIpPort() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);// 网络字节序转主机字节序
    // int snprintf(char *str, size_t size, const char *format,...)
    snprintf(buf+end, sizeof(buf)-end, ":%u", port);
    return buf;
}

std::string InetAddress::toIpPort() const {
    return ntohs(addr_.sin_port);
}
