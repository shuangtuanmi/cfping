#include "cidrexpander.h"
#include "iputils.h"

// 构造函数，初始化成员变量
CidrExpander::CidrExpander(QObject *parent)
    : QObject(parent)
    , m_totalIPs(0)
    , m_processedIPs(0)
{
}

// 设置CIDR范围
void CidrExpander::setCidrRanges(const QStringList& cidrRanges)
{
    // 清空已有的范围
    while (!m_ranges.empty()) {
        m_ranges.pop();
    }
    
    m_totalIPs = 0;
    m_processedIPs = 0;
    
    // 处理每个CIDR范围
    for (const QString& cidr : cidrRanges) {
        if (!IPUtils::isValidCIDR(cidr)) {
            continue; // 跳过无效的CIDR
        }
        
        // 先计算IP数量
        uint64_t count = IPUtils::getCIDRIPCount(cidr);
        if (count == UINT64_MAX) {
            // IPv6范围太大，设置为合理的上限
            count = 1000000; // 可根据需要调整
        } else if (count == 0) {
            // 无效的CIDR，跳过
            continue;
        }
        
        // 获取起始和结束IP
        auto range = IPUtils::cidrToRange(cidr);
        m_ranges.emplace(range.first, range.second, cidr);
        
        // 累加IP数量
        m_totalIPs += count;
    }
}

// 判断是否还有未处理的IP
bool CidrExpander::hasMore() const
{
    return !m_ranges.empty();
}

// 获取下一个批次的IP地址
QStringList CidrExpander::getNextBatch(int batchSize)
{
    QStringList batch;
    
    while (batch.size() < batchSize && !m_ranges.empty()) {
        auto& range = m_ranges.front();
        
        // 从当前范围添加IP到批次
        while (batch.size() < batchSize && IPUtils::compareIP(range.current, range.end)) {
            batch.append(IPUtils::ipToString(range.current));
            range.current = IPUtils::incrementIP(range.current);
            m_processedIPs++;
            
            // 确保处理IP数不超过总IP数
            if (m_processedIPs.load() >= m_totalIPs.load()) {
                break;
            }
        }
        
        // 如果当前范围已处理完，移除它
        if (!IPUtils::compareIP(range.current, range.end) || m_processedIPs.load() >= m_totalIPs.load()) {
            m_ranges.pop();
        }
        
        // 如果已经处理了足够的IP，提前结束
        if (m_processedIPs.load() >= m_totalIPs.load()) {
            break;
        }
    }
    
    // 如果批次不为空，发送进度信号
    if (!batch.isEmpty()) {
        emit expansionProgress(m_processedIPs.load(), m_totalIPs.load());
    }
    
    return batch;
}

// 获取总IP数量
uint64_t CidrExpander::getTotalIPCount() const
{
    return m_totalIPs.load();
}

// 获取已处理的IP数量
uint64_t CidrExpander::getProcessedIPCount() const
{
    return m_processedIPs.load();
}
