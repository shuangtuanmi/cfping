#ifndef IPUTILS_H
#define IPUTILS_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <cstdint>
#include <vector>
#include <chrono>

class IPUtils
{
public:
    static bool isValidIP(const QString& ip);
    static bool isValidCIDR(const QString& cidr);
    static uint32_t ipToUint32(const QString& ip);
    static QString uint32ToIP(uint32_t ip);
    static std::pair<uint32_t, uint32_t> cidrToRange(const QString& cidr);
    static uint64_t getCIDRIPCount(const QString& cidr);
    static QStringList expandCIDR(const QString& cidr, int maxIPs = -1);
    static double calculateLatency(const std::chrono::steady_clock::time_point& start,
                                 const std::chrono::steady_clock::time_point& end);
};

#endif // IPUTILS_H
