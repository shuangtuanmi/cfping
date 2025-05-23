#ifndef PINGWORKER_H
#define PINGWORKER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
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

    void setSettings(int threadCount, int timeoutMs, bool enableLogging, int maxConcurrentTasks);

public slots:
    void startPing(const QStringList& cidrRanges);
    void stopPing();

signals:
    void pingResult(const QString& ip, double latency, bool success);
    void progress(int current, int total);
    void logMessage(const QString& message);
    void finished();

private:
    boost::asio::awaitable<void> pingIPWithAddress(boost::asio::ip::address address, QString originalIP);
    boost::asio::awaitable<void> processPingQueue();
    void processNextBatch();
    void cleanup();
    void safeCleanup();
    
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_workGuard;
    std::unique_ptr<boost::asio::cancellation_signal> m_cancellationSignal;
    std::vector<std::thread> m_threads;
    std::unique_ptr<CidrExpander> m_cidrExpander;
    
    std::queue<QString> m_ipQueue;
    std::mutex m_queueMutex;
    
    QTimer* m_batchTimer;
    QTimer* m_stopCheckTimer;
    QTimer* m_cleanupTimer;
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopRequested;
    std::atomic<bool> m_cleanupInProgress;
    std::atomic<int> m_completedCount;
    std::atomic<int> m_totalCount;
    std::atomic<int> m_activePings;
    
    int m_threadCount;
    int m_timeoutMs;
    int m_maxConcurrentTasks;
    bool m_enableLogging;
    
    static constexpr int BATCH_SIZE = 500;
    static constexpr int DEFAULT_MAX_CONCURRENT_PINGS = 1000;
};

#endif // PINGWORKER_H
