#include "pingworker.h"
#include "cidrexpander.h"
#include "iputils.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <thread>

PingWorker::PingWorker(QObject *parent)
    : QObject(parent)
    , m_ioContext(std::make_unique<boost::asio::io_context>())
    , m_cidrExpander(std::make_unique<CidrExpander>(this))
    , m_batchTimer(new QTimer(this))
    , m_stopCheckTimer(new QTimer(this))
    , m_running(false)
    , m_stopRequested(false)
    , m_completedCount(0)
    , m_totalCount(0)
    , m_activePings(0)
    , m_threadCount(4)
    , m_timeoutMs(1000)
    , m_enableLogging(false)
{
    connect(m_batchTimer, &QTimer::timeout, this, &PingWorker::processNextBatch);
    m_batchTimer->setInterval(50); // 更频繁的批次处理
    
    // 新增：停止检查定时器
    connect(m_stopCheckTimer, &QTimer::timeout, this, [this]() {
        if (m_stopRequested.load()) {
            cleanup();
        }
    });
    m_stopCheckTimer->setInterval(100); // 每100ms检查一次停止状态
}

PingWorker::~PingWorker()
{
    stopPing();
}

void PingWorker::setSettings(int threadCount, int timeoutMs, bool enableLogging)
{
    m_threadCount = threadCount;
    m_timeoutMs = timeoutMs;
    m_enableLogging = enableLogging;
}

void PingWorker::startPing(const QStringList& cidrRanges)
{
    if (m_running.load()) return;
    
    m_running = true;
    m_stopRequested = false;
    m_completedCount = 0;
    m_activePings = 0;
    
    // Setup CIDR expander
    m_cidrExpander->setCidrRanges(cidrRanges);
    m_totalCount = static_cast<int>(m_cidrExpander->getTotalIPCount());
    
    emit logMessage(QString("Starting ping test for %1 IP addresses with %2 threads")
                   .arg(m_totalCount).arg(m_threadCount));
    
    // Create work guard to keep io_context alive
    m_workGuard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        m_ioContext->get_executor());
    
    // Start worker threads
    m_threads.clear();
    for (int i = 0; i < m_threadCount; ++i) {
        m_threads.emplace_back([this]() {
            try {
                m_ioContext->run();
            } catch (const std::exception& e) {
                emit logMessage(QString("Worker thread error: %1").arg(e.what()));
            }
        });
    }
    
    // 启动定时器
    m_stopCheckTimer->start();
    m_batchTimer->start();
    processNextBatch();
}

void PingWorker::stopPing()
{
    if (!m_running.load()) return;
    
    emit logMessage("Stop request received...");
    m_stopRequested = true;
    
    // 立即停止批次处理
    m_batchTimer->stop();
    
    // 使用定时器异步执行清理，避免阻塞
    QTimer::singleShot(0, this, [this]() {
        cleanup();
    });
}

void PingWorker::cleanup()
{
    if (!m_running.load()) return;
    
    emit logMessage("Cleaning up...");
    m_running = false;
    m_stopCheckTimer->stop();
    
    // Stop io_context
    if (m_workGuard) {
        m_workGuard.reset();
    }
    
    if (m_ioContext) {
        m_ioContext->stop();
    }
    
    // Wait for threads to finish with timeout
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            // 使用detach而不是join，避免长时间阻塞
            thread.detach();
        }
    }
    m_threads.clear();
    
    // 清空队列
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<QString> empty;
        m_ipQueue.swap(empty);
    }
    
    // Reset io_context for next use
    m_ioContext = std::make_unique<boost::asio::io_context>();
    
    emit finished();
}

void PingWorker::processNextBatch()
{
    // 频繁检查停止状态
    if (m_stopRequested.load() || !m_running.load()) {
        return;
    }
    
    // 限制并发ping数量
    if (m_activePings.load() >= MAX_CONCURRENT_PINGS) {
        return;
    }
    
    if (!m_cidrExpander->hasMore()) {
        // 等待活跃ping完成
        if (m_activePings.load() == 0) {
            m_batchTimer->stop();
            QTimer::singleShot(1000, this, [this]() {
                if (m_running.load() && !m_stopRequested.load()) {
                    cleanup();
                }
            });
        }
        return;
    }
    
    QStringList batch = m_cidrExpander->getNextBatch(BATCH_SIZE);
    
    for (const QString& ip : batch) {
        // 再次检查停止状态
        if (m_stopRequested.load()) {
            break;
        }
        
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_ipQueue.push(ip);
        }
        
        // 增加活跃ping计数
        m_activePings++;
        
        // Spawn coroutine for this IP
        boost::asio::co_spawn(*m_ioContext, 
                             processPingQueue(), 
                             boost::asio::detached);
    }
    
    emit progress(static_cast<int>(m_cidrExpander->getProcessedIPCount()), m_totalCount);
}

boost::asio::awaitable<void> PingWorker::processPingQueue()
{
    QString ip;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_ipQueue.empty()) {
            m_activePings--;
            co_return;
        }
        ip = m_ipQueue.front();
        m_ipQueue.pop();
    }
    
    if (!ip.isEmpty() && !m_stopRequested.load()) {
        co_await pingIP(ip);
    }
    
    // 减少活跃ping计数
    m_activePings--;
}

boost::asio::awaitable<void> PingWorker::pingIP(const QString& ip)
{
    try {
        // 早期检查停止状态
        if (m_stopRequested.load()) {
            co_return;
        }
        
        auto executor = co_await boost::asio::this_coro::executor;
        boost::asio::ip::tcp::socket socket(executor);
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Create endpoint for HTTP port (80) - commonly open on CDN servers
        boost::system::error_code ec;
        auto address = boost::asio::ip::make_address(ip.toStdString(), ec);
        
        if (ec || m_stopRequested.load()) {
            if (!m_stopRequested.load()) {
                emit pingResult(ip, 0.0, false);
                if (m_enableLogging) {
                    emit logMessage(QString("Invalid IP address: %1").arg(ip));
                }
            }
            m_completedCount++;
            co_return;
        }
        
        boost::asio::ip::tcp::endpoint endpoint(address, 80);
        
        // Setup timeout - 使用更短的超时时间提高响应性
        boost::asio::steady_timer timer(executor);
        timer.expires_after(std::chrono::milliseconds(std::min(m_timeoutMs, 2000)));
        
        // Use experimental awaitable operators for race condition
        using namespace boost::asio::experimental::awaitable_operators;
        
        try {
            // 在连接前再次检查停止状态
            if (m_stopRequested.load()) {
                co_return;
            }
            
            // Try to connect with timeout
            auto result = co_await (
                socket.async_connect(endpoint, boost::asio::use_awaitable) ||
                timer.async_wait(boost::asio::use_awaitable)
            );
            
            // 连接后检查停止状态
            if (m_stopRequested.load()) {
                boost::system::error_code close_ec;
                socket.close(close_ec);
                co_return;
            }
            
            auto end_time = std::chrono::steady_clock::now();
            double latency = IPUtils::calculateLatency(start_time, end_time);
            
            bool success = result.index() == 0; // First alternative completed (connect, not timeout)
            
            if (success) {
                // Close the socket gracefully
                boost::system::error_code close_ec;
                socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, close_ec);
                socket.close(close_ec);
            }
            
            if (!m_stopRequested.load()) {
                emit pingResult(ip, latency, success);
                
                if (m_enableLogging) {
                    if (success) {
                        emit logMessage(QString("TCP connect %1:80: %2ms").arg(ip).arg(latency, 0, 'f', 2));
                    } else {
                        emit logMessage(QString("TCP connect %1:80 timeout").arg(ip));
                    }
                }
            }
            
        } catch (const boost::system::system_error& e) {
            if (m_stopRequested.load()) {
                co_return;
            }
            
            auto end_time = std::chrono::steady_clock::now();
            double latency = IPUtils::calculateLatency(start_time, end_time);
            
            // Check if it's a "connection refused" error, which means the IP is reachable
            // but the port is closed - this is still a valid response for CDN testing
            bool isReachable = (e.code() == boost::asio::error::connection_refused);
            
            emit pingResult(ip, latency, isReachable);
            
            if (m_enableLogging) {
                if (isReachable) {
                    emit logMessage(QString("TCP connect %1:80: %2ms (port closed but reachable)")
                                   .arg(ip).arg(latency, 0, 'f', 2));
                } else {
                    emit logMessage(QString("TCP connect %1:80 failed: %2")
                                   .arg(ip, e.what()));
                }
            }
        }
        
    } catch (const std::exception& e) {
        if (!m_stopRequested.load()) {
            emit pingResult(ip, 0.0, false);
            if (m_enableLogging) {
                emit logMessage(QString("TCP connect %1 failed: %2").arg(ip, e.what()));
            }
        }
    }
    
    m_completedCount++;
}
