#ifndef CIDREXPANDER_H
#define CIDREXPANDER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "iputils.h"
#include <queue>
#include <atomic>

// CIDR扩展器类，用于将CIDR范围展开为IP列表
class CidrExpander : public QObject
{
    Q_OBJECT

public:
    explicit CidrExpander(QObject *parent = nullptr);
    
    // 设置CIDR范围
    void setCidrRanges(const QStringList& cidrRanges);
    // 判断是否还有未处理的IP
    bool hasMore() const;
    // 获取下一个批次的IP地址
    QStringList getNextBatch(int batchSize = 1000);
    // 获取总IP数量
    uint64_t getTotalIPCount() const;
    // 获取已处理的IP数量
    uint64_t getProcessedIPCount() const;

signals:
    // 扩展进度信号，参数为已处理和总数
    void expansionProgress(uint64_t processed, uint64_t total);

private:
    // CIDR范围结构体，支持IPv4和IPv6
    struct CidrRange {
        IPAddress start;        // 起始IP
        IPAddress end;          // 结束IP
        IPAddress current;      // 当前处理到的IP
        QString originalCidr;   // 原始CIDR字符串
        
        CidrRange(const IPAddress& s, const IPAddress& e, const QString& cidr)
            : start(s), end(e), current(s), originalCidr(cidr) {}
    };
    
    std::queue<CidrRange> m_ranges;         // CIDR范围队列
    std::atomic<uint64_t> m_totalIPs;       // 总IP数量
    std::atomic<uint64_t> m_processedIPs;   // 已处理IP数量
};

#endif // CIDREXPANDER_H
