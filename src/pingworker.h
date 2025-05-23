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

// PingWorker类，负责批量异步TCP连接测试
class PingWorker : public QObject
{
    Q_OBJECT

public:
    explicit PingWorker(QObject *parent = nullptr); // 构造函数
    ~PingWorker(); // 析构函数

    // 设置线程数、超时时间、日志开关、最大并发任务数
    void setSettings(int threadCount, int timeoutMs, bool enableLogging, int maxConcurrentTasks);

public slots:
    void startPing(const QStringList& cidrRanges); // 启动ping任务
    void stopPing(); // 停止ping任务

signals:
    void pingResult(const QString& ip, double latency, bool success); // 单个IP测试结果
    void progress(int current, int total); // 进度信号
    void logMessage(const QString& message); // 日志信号
    void finished(); // 任务完成信号

private:
    // 协程：对单个IP进行TCP连接测试
    boost::asio::awaitable<void> pingIPWithAddress(boost::asio::ip::address address, QString originalIP);

    // 处理下一批IP
    void processNextBatch();
    // 清理资源
    void cleanup();
    // 安全清理，防止重复
    void safeCleanup();
    
    // Boost Asio相关成员
    std::unique_ptr<boost::asio::io_context> m_ioContext; // IO上下文
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_workGuard; // 保持io_context存活
    std::unique_ptr<boost::asio::cancellation_signal> m_cancellationSignal; // 协程取消信号
    std::vector<std::thread> m_threads; // 线程池
    std::unique_ptr<CidrExpander> m_cidrExpander; // CIDR扩展器
    
    // IP队列与互斥锁
    std::queue<QString> m_ipQueue;
    std::mutex m_queueMutex;
    
    // 定时器
    QTimer* m_batchTimer; // 批量处理定时器
    QTimer* m_stopCheckTimer; // 停止检查定时器
    QTimer* m_cleanupTimer; // 清理定时器
    
    // 状态变量
    std::atomic<bool> m_running; // 是否正在运行
    std::atomic<bool> m_stopRequested; // 是否请求停止
    std::atomic<bool> m_cleanupInProgress; // 是否正在清理
    std::atomic<int> m_completedCount; // 已完成数量
    std::atomic<int> m_totalCount; // 总数量
    std::atomic<int> m_activePings; // 活跃任务数
    
    int m_threadCount; // 线程数
    int m_timeoutMs; // 超时时间
    int m_maxConcurrentTasks; // 最大并发任务数
    bool m_enableLogging; // 是否启用日志
    
    static constexpr int BATCH_SIZE = 500; // 每批处理数量
    static constexpr int DEFAULT_MAX_CONCURRENT_PINGS = 1000; // 默认最大并发数
};

#endif // PINGWORKER_H
