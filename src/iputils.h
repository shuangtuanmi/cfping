#ifndef IPUTILS_H
#define IPUTILS_H

#include <QString>
#include <QStringList>
#include <cstdint>
#include <chrono>
#include <array>

// IP地址结构体，支持IPv4和IPv6
struct IPAddress {
    enum Type { IPv4, IPv6 };
    Type type;
    union {
        uint32_t ipv4;
        std::array<uint8_t, 16> ipv6;
    };
    
    IPAddress() : type(IPv4), ipv4(0) {}
    IPAddress(uint32_t ip) : type(IPv4), ipv4(ip) {}
    IPAddress(const std::array<uint8_t, 16>& ip) : type(IPv6), ipv6(ip) {}
};

// IP工具类，提供IP和CIDR相关的静态方法
class IPUtils
{
public:
    // 判断IP地址是否合法
    static bool isValidIP(const QString& ip);
    // 判断CIDR格式是否合法
    static bool isValidCIDR(const QString& cidr);
    // IP字符串转为IPAddress结构
    static IPAddress stringToIP(const QString& ip);
    // IPAddress结构转为IP字符串
    static QString ipToString(const IPAddress& ip);
    // CIDR转为起始和结束IP
    static std::pair<IPAddress, IPAddress> cidrToRange(const QString& cidr);
    // 获取CIDR范围内的IP数量
    static uint64_t getCIDRIPCount(const QString& cidr);
    // 展开CIDR为IP列表
    static QStringList expandCIDR(const QString& cidr, int maxIPs = -1);
    // 计算延迟（毫秒）
    static double calculateLatency(const std::chrono::steady_clock::time_point& start,
                                 const std::chrono::steady_clock::time_point& end);
    
    // 兼容性方法（仅IPv4）
    static uint32_t ipToUint32(const QString& ip);
    static QString uint32ToIP(uint32_t ip);
    
    // IPv6专用方法
    static bool isIPv6(const QString& ip);
    static std::array<uint8_t, 16> ipv6ToBytes(const QString& ipv6);
    static QString bytesToIPv6(const std::array<uint8_t, 16>& bytes);
    static IPAddress incrementIP(const IPAddress& ip);
    static bool compareIP(const IPAddress& ip1, const IPAddress& ip2);
};

#endif // IPUTILS_H
