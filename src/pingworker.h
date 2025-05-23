#ifndef PINGWORKER_H
#define PINGWORKER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <memory>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>

class CidrExpander;

class PingWorker : public QObject
{
    Q_OBJECT

public:
    explicit PingWorker(QObject *parent = nullptr);
    ~PingWorker();

    void setSettings(int threadCount, int timeoutMs, bool enableLogging);

public slots:
    void startPing(const QStringList& cidrRanges);
    void stopPing();

signals:
    void pingResult(const QString& ip, double latency, bool success);
    void progress(int current, int total);
    void logMessage(const QString& message);
    void finished();

private:
    // TCP connection test instead of ICMP ping (no admin rights required)
    boost::asio::awaitable<void> pingIP(const QString& ip);
    boost::asio::awaitable<void> processPingQueue();
    void processNextBatch();
    void cleanup();
    
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_workGuard;
    std::vector<std::thread> m_threads;
    std::unique_ptr<CidrExpander> m_cidrExpander;
    
    std::queue<QString> m_ipQueue;
    std::mutex m_queueMutex;
    
    QTimer* m_batchTimer;
    QTimer* m_stopCheckTimer;  //定期检查停止状态
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopRequested;  //停止请求标志
    std::atomic<int> m_completedCount;
    std::atomic<int> m_totalCount;
    std::atomic<int> m_activePings;  //活跃ping计数
    
    int m_threadCount;
    int m_timeoutMs;
    bool m_enableLogging;
    
    static constexpr int BATCH_SIZE = 500;  //减小批次大小以提高响应性
    static constexpr int MAX_CONCURRENT_PINGS = 1000;  //限制并发ping数量
};

#endif // PINGWORKER_H
