#include "cidrexpander.h"
#include "iputils.h"

CidrExpander::CidrExpander(QObject *parent)
    : QObject(parent)
    , m_totalIPs(0)
    , m_processedIPs(0)
{
}

void CidrExpander::setCidrRanges(const QStringList& cidrRanges)
{
    // Clear existing ranges
    while (!m_ranges.empty()) {
        m_ranges.pop();
    }
    
    m_totalIPs = 0;
    m_processedIPs = 0;
    
    // Process each CIDR range
    for (const QString& cidr : cidrRanges) {
        if (!IPUtils::isValidCIDR(cidr)) {
            continue;
        }
        
        auto range = IPUtils::cidrToRange(cidr);
        m_ranges.emplace(range.first, range.second, cidr);
        m_totalIPs += (range.second - range.first + 1);
    }
}

bool CidrExpander::hasMore() const
{
    return !m_ranges.empty();
}

QStringList CidrExpander::getNextBatch(int batchSize)
{
    QStringList batch;
    
    while (batch.size() < batchSize && !m_ranges.empty()) {
        auto& range = m_ranges.front();
        
        // Add IPs from current range to batch
        while (batch.size() < batchSize && range.current <= range.end) {
            batch.append(IPUtils::uint32ToIP(range.current));
            range.current++;
            m_processedIPs++;
        }
        
        // If range is exhausted, remove it
        if (range.current > range.end) {
            m_ranges.pop();
        }
    }
    
    if (!batch.isEmpty()) {
        emit expansionProgress(m_processedIPs.load(), m_totalIPs.load());
    }
    
    return batch;
}

uint64_t CidrExpander::getTotalIPCount() const
{
    return m_totalIPs.load();
}

uint64_t CidrExpander::getProcessedIPCount() const
{
    return m_processedIPs.load();
}
