#include "iputils.h"
#include <QtCore/QRegularExpression>
#include <QtNetwork/QHostAddress>
#include <cmath>
#include <chrono>

// 判断IP地址是否合法
bool IPUtils::isValidIP(const QString& ip)
{
    QHostAddress address(ip);
    return !address.isNull() && (address.protocol() == QAbstractSocket::IPv4Protocol || address.protocol() == QAbstractSocket::IPv6Protocol);
}

// 判断CIDR格式是否合法
bool IPUtils::isValidCIDR(const QString& cidr)
{
    QStringList parts = cidr.split('/');
    if (parts.size() != 2) return false;
    
    bool ok;
    int prefix = parts[1].toInt(&ok);
    if (!ok) return false;
    
    // 检查是否为IPv6
    if (isIPv6(parts[0])) {
        return prefix >= 0 && prefix <= 128 && isValidIP(parts[0]);
    } else {
        return prefix >= 0 && prefix <= 32 && isValidIP(parts[0]);
    }
}

// 判断是否为IPv6地址
bool IPUtils::isIPv6(const QString& ip)
{
    return ip.contains(':');
}

// IP字符串转为IPAddress结构
IPAddress IPUtils::stringToIP(const QString& ip)
{
    if (isIPv6(ip)) {
        return IPAddress(ipv6ToBytes(ip));
    } else {
        return IPAddress(ipToUint32(ip));
    }
}

// IPAddress结构转为IP字符串
QString IPUtils::ipToString(const IPAddress& ip)
{
    if (ip.type == IPAddress::IPv6) {
        return bytesToIPv6(ip.ipv6);
    } else {
        return uint32ToIP(ip.ipv4);
    }
}

// IPv6字符串转为字节数组
std::array<uint8_t, 16> IPUtils::ipv6ToBytes(const QString& ipv6)
{
    QHostAddress addr(ipv6);
    std::array<uint8_t, 16> result = {};
    if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        Q_IPV6ADDR ipv6Addr = addr.toIPv6Address();
        memcpy(result.data(), ipv6Addr.c, 16);
    }
    return result;
}

// 字节数组转为IPv6字符串
QString IPUtils::bytesToIPv6(const std::array<uint8_t, 16>& bytes)
{
    Q_IPV6ADDR ipv6Addr;
    memcpy(ipv6Addr.c, bytes.data(), 16);
    QHostAddress addr(ipv6Addr);
    return addr.toString();
}

// IP地址递增
IPAddress IPUtils::incrementIP(const IPAddress& ip)
{
    if (ip.type == IPAddress::IPv4) {
        return IPAddress(ip.ipv4 + 1);
    } else {
        std::array<uint8_t, 16> result = ip.ipv6;
        for (int i = 15; i >= 0; --i) {
            if (result[i] < 255) {
                result[i]++;
                break;
            } else {
                result[i] = 0;
            }
        }
        return IPAddress(result);
    }
}

// 比较IP地址
bool IPUtils::compareIP(const IPAddress& ip1, const IPAddress& ip2)
{
    if (ip1.type != ip2.type) return false;
    
    if (ip1.type == IPAddress::IPv4) {
        return ip1.ipv4 <= ip2.ipv4;
    } else {
        return memcmp(ip1.ipv6.data(), ip2.ipv6.data(), 16) <= 0;
    }
}

// 将CIDR转换为起始和结束IP
std::pair<IPAddress, IPAddress> IPUtils::cidrToRange(const QString& cidr)
{
    QStringList parts = cidr.split('/');
    if (parts.size() != 2) return {IPAddress(), IPAddress()};
    
    int prefix = parts[1].toInt();
    IPAddress baseIP = stringToIP(parts[0]);
    
    if (baseIP.type == IPAddress::IPv4) {
        if (prefix < 0 || prefix > 32) return {IPAddress(), IPAddress()};
        
        uint32_t mask = (prefix == 0) ? 0 : (~0U << (32 - prefix));
        uint32_t start = baseIP.ipv4 & mask;
        uint32_t end = start | (~mask);
        
        return {IPAddress(start), IPAddress(end)};
    } else {
        if (prefix < 0 || prefix > 128) return {IPAddress(), IPAddress()};
        
        std::array<uint8_t, 16> start = baseIP.ipv6;
        std::array<uint8_t, 16> end = baseIP.ipv6;
        
        // 应用前缀掩码
        int bytes = prefix / 8;
        int bits = prefix % 8;
        
        // 清除网络部分之后的位
        for (int i = bytes; i < 16; ++i) {
            if (i == bytes && bits > 0) {
                uint8_t mask = 0xFF << (8 - bits);
                start[i] &= mask;
                end[i] |= ~mask;
            } else if (i > bytes) {
                start[i] = 0;
                end[i] = 0xFF;
            }
        }
        
        return {IPAddress(start), IPAddress(end)};
    }
}

// 获取CIDR范围内的IP数量
uint64_t IPUtils::getCIDRIPCount(const QString& cidr)
{
    QStringList parts = cidr.split('/');
    if (parts.size() != 2) return 0;
    
    bool ok;
    int prefix = parts[1].toInt(&ok);
    if (!ok) return 0;
    
    if (isIPv6(parts[0])) {
        if (prefix < 0 || prefix > 128) return 0;
        if (prefix <= 64) {
            return UINT64_MAX; // 范围太大，返回最大值
        }
        // 计算2的(128-prefix)次方
        return (prefix == 128) ? 1 : (static_cast<uint64_t>(1) << (128 - prefix));
    } else {
        if (prefix < 0 || prefix > 32) return 0;
        // 对于IPv4，计算2的(32-prefix)次方
        return (prefix == 32) ? 1 : (static_cast<uint64_t>(1) << (32 - prefix));
    }
}

// IP字符串转为32位无符号整数
uint32_t IPUtils::ipToUint32(const QString& ip)
{
    QStringList parts = ip.split('.');
    if (parts.size() != 4) return 0;
    
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        bool ok;
        uint32_t octet = parts[i].toUInt(&ok);
        if (!ok || octet > 255) return 0;
        result = (result << 8) | octet;
    }
    return result;
}

// 32位无符号整数转为IP字符串
QString IPUtils::uint32ToIP(uint32_t ip)
{
    return QString("%1.%2.%3.%4")
        .arg((ip >> 24) & 0xFF)
        .arg((ip >> 16) & 0xFF)
        .arg((ip >> 8) & 0xFF)
        .arg(ip & 0xFF);
}

// 展开CIDR为IP列表，最多maxIPs个
QStringList IPUtils::expandCIDR(const QString& cidr, int maxIPs)
{
    QStringList result;
    auto range = cidrToRange(cidr);
    
    IPAddress current = range.first;
    int count = 0;
    
    while (compareIP(current, range.second) && (maxIPs <= 0 || count < maxIPs)) {
        result.append(ipToString(current));
        current = incrementIP(current);
        count++;
    }
    
    return result;
}

// 计算延迟（毫秒）
double IPUtils::calculateLatency(const std::chrono::steady_clock::time_point& start,
                               const std::chrono::steady_clock::time_point& end)
{
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0; // 转换为毫秒
}
