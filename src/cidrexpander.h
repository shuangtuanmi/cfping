#ifndef CIDREXPANDER_H
#define CIDREXPANDER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <memory>
#include <queue>
#include <atomic>

class CidrExpander : public QObject
{
    Q_OBJECT

public:
    explicit CidrExpander(QObject *parent = nullptr);
    
    void setCidrRanges(const QStringList& cidrRanges);
    bool hasMore() const;
    QStringList getNextBatch(int batchSize = 1000);
    uint64_t getTotalIPCount() const;
    uint64_t getProcessedIPCount() const;

signals:
    void expansionProgress(uint64_t processed, uint64_t total);

private:
    struct CidrRange {
        uint32_t start;
        uint32_t end;
        uint32_t current;
        QString originalCidr;
        
        CidrRange(uint32_t s, uint32_t e, const QString& cidr)
            : start(s), end(e), current(s), originalCidr(cidr) {}
    };
    
    std::queue<CidrRange> m_ranges;
    std::atomic<uint64_t> m_totalIPs;
    std::atomic<uint64_t> m_processedIPs;
};

#endif // CIDREXPANDER_H
