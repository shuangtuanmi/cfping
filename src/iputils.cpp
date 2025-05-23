#include "iputils.h"
#include <QtCore/QRegularExpression>
#include <QtNetwork/QHostAddress>
#include <cmath>
#include <chrono>

bool IPUtils::isValidIP(const QString& ip)
{
    QHostAddress address(ip);
    return !address.isNull() && address.protocol() == QAbstractSocket::IPv4Protocol;
}

bool IPUtils::isValidCIDR(const QString& cidr)
{
    static QRegularExpression cidrRegex(R"(^(\d{1,3}\.){3}\d{1,3}/\d{1,2}$)");
    if (!cidrRegex.match(cidr).hasMatch()) {
        return false;
    }
    
    QStringList parts = cidr.split('/');
    if (parts.size() != 2) return false;
    
    bool ok;
    int prefix = parts[1].toInt(&ok);
    if (!ok || prefix < 0 || prefix > 32) return false;
    
    return isValidIP(parts[0]);
}

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

QString IPUtils::uint32ToIP(uint32_t ip)
{
    return QString("%1.%2.%3.%4")
        .arg((ip >> 24) & 0xFF)
        .arg((ip >> 16) & 0xFF)
        .arg((ip >> 8) & 0xFF)
        .arg(ip & 0xFF);
}

std::pair<uint32_t, uint32_t> IPUtils::cidrToRange(const QString& cidr)
{
    QStringList parts = cidr.split('/');
    if (parts.size() != 2) return {0, 0};
    
    uint32_t baseIP = ipToUint32(parts[0]);
    int prefix = parts[1].toInt();
    
    if (prefix < 0 || prefix > 32) return {0, 0};
    
    uint32_t mask = (prefix == 0) ? 0 : (~0U << (32 - prefix));
    uint32_t start = baseIP & mask;
    uint32_t end = start | (~mask);
    
    return {start, end};
}

uint64_t IPUtils::getCIDRIPCount(const QString& cidr)
{
    QStringList parts = cidr.split('/');
    if (parts.size() != 2) return 0;
    
    int prefix = parts[1].toInt();
    if (prefix < 0 || prefix > 32) return 0;
    
    return static_cast<uint64_t>(1) << (32 - prefix);
}

QStringList IPUtils::expandCIDR(const QString& cidr, int maxIPs)
{
    QStringList result;
    auto range = cidrToRange(cidr);
    uint64_t count = range.second - range.first + 1;
    
    if (maxIPs > 0 && count > static_cast<uint64_t>(maxIPs)) {
        count = maxIPs;
    }
    
    for (uint64_t i = 0; i < count; ++i) {
        result.append(uint32ToIP(range.first + i));
    }
    
    return result;
}

double IPUtils::calculateLatency(const std::chrono::steady_clock::time_point& start,
                               const std::chrono::steady_clock::time_point& end)
{
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0; // Convert to milliseconds
}
