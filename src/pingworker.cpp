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

// PingWorker 构造函数，初始化成员变量和定时器
PingWorker::PingWorker(QObject *parent)
    : QObject(parent)
    , m_ioContext(std::make_unique<boost::asio::io_context>())
    , m_cancellationSignal(std::make_unique<boost::asio::cancellation_signal>())
    , m_cidrExpander(std::make_unique<CidrExpander>(this))
    , m_batchTimer(new QTimer(this))
    , m_stopCheckTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_running(false)
    , m_stopRequested(false)
    , m_cleanupInProgress(false)
    , m_completedCount(0)
    , m_totalCount(0)
    , m_activePings(0)
    , m_threadCount(4)
    , m_timeoutMs(1000)
    , m_maxConcurrentTasks(DEFAULT_MAX_CONCURRENT_PINGS)
    , m_port(80)  // 默认端口80
    , m_enableLogging(false)
{
    // 批量处理定时器，定期处理下一批IP
    connect(m_batchTimer, &QTimer::timeout, this, &PingWorker::processNextBatch);
    m_batchTimer->setInterval(10); // 更频繁的任务检查
    
    // 停止检查定时器，定期检查是否需要安全清理
    connect(m_stopCheckTimer, &QTimer::timeout, this, [this]() {
        if (m_stopRequested.load() && !m_cleanupInProgress.load()) {
            safeCleanup();
        }
    });
    m_stopCheckTimer->setInterval(100); // 每100ms检查一次停止状态
    
    // 延迟清理定时器，到时间后执行清理
    connect(m_cleanupTimer, &QTimer::timeout, this, [this]() {
        cleanup();
    });
    m_cleanupTimer->setSingleShot(true);
}

// 析构函数，确保安全停止和清理
PingWorker::~PingWorker()
{
    if (m_running.load()) {
        stopPing();
        // 等待清理完成，最多3秒
        int waitCount = 0;
        while (m_running.load() && waitCount < 30) { // 最多等待3秒
            QThread::msleep(100);
            waitCount++;
        }
    }
}

// 设置线程数、超时时间、日志开关、最大并发任务数、端口号
void PingWorker::setSettings(int threadCount, int timeoutMs, bool enableLogging, int maxConcurrentTasks, int port)
{
    m_threadCount = threadCount;
    m_timeoutMs = timeoutMs;
    m_enableLogging = enableLogging;
    m_maxConcurrentTasks = maxConcurrentTasks > 0 ? maxConcurrentTasks : DEFAULT_MAX_CONCURRENT_PINGS;
    m_port = port > 0 && port <= 65535 ? port : 80;  // 验证端口号范围
}

// 启动ping任务，初始化环境并启动线程池
void PingWorker::startPing(const QStringList& cidrRanges)
{
    if (m_running.load() || m_cleanupInProgress.load()) return;
    
    m_running = true;
    m_stopRequested = false;
    m_cleanupInProgress = false;
    m_completedCount = 0;
    m_activePings = 0;
    
    // 重置取消信号
    m_cancellationSignal = std::make_unique<boost::asio::cancellation_signal>();
    
    // 设置CIDR范围并获取总IP数
    m_cidrExpander->setCidrRanges(cidrRanges);
    m_totalCount = static_cast<int>(m_cidrExpander->getTotalIPCount());
    
    // 确保总数至少为1，防止除零错误
    if (m_totalCount <= 0) {
        m_totalCount = 1;
    }
    
    emit logMessage(QString("Starting TCP connection test for %1 IP addresses with %2 threads (IPv4/IPv6 supported)")
                   .arg(m_totalCount).arg(m_threadCount));
    
    // 创建io_context的work guard，防止提前退出
    m_workGuard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        m_ioContext->get_executor());
    
    m_threads.clear();
    // 启动线程池
    for (int i = 0; i < m_threadCount; ++i) {
        m_threads.emplace_back([this]() {
            try {
                // 设置线程局部的异常处理
                while (!m_stopRequested.load() && !m_ioContext->stopped()) {
                    try {
                        m_ioContext->run();
                        break; // 正常退出
                    } catch (const boost::system::system_error& e) {
                        // 忽略取消相关的错误
                        if (e.code() != boost::asio::error::operation_aborted) {
                            emit logMessage(QString("Worker thread system error: %1").arg(e.code().value()));
                        }
                        if (m_stopRequested.load()) break;
                    } catch (const std::exception& e) {
                        emit logMessage(QString("Worker thread error: %1").arg(e.what()));
                        if (m_stopRequested.load()) break;
                    }
                    
                    // 短暂休眠后重试
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } catch (...) {
                emit logMessage("Worker thread encountered unexpected error");
            }
        });
    }
    
    m_stopCheckTimer->start();
    m_batchTimer->start();
    processNextBatch();
}

// 停止ping任务，发出取消信号并延迟清理
void PingWorker::stopPing()
{
    if (!m_running.load() || m_cleanupInProgress.load()) return;
    
    emit logMessage("Stop request received...");
    m_stopRequested = true;
    
    // 立即停止批次处理
    m_batchTimer->stop();
    
    // 发送取消信号给所有协程
    if (m_cancellationSignal) {
        try {
            #undef emit
            m_cancellationSignal->boost::asio::cancellation_signal::emit(boost::asio::cancellation_type::all);
            #define emit Q_EMIT
        } catch (...) {
            // 忽略取消信号错误
        }
    }
    
    // 使用定时器延迟执行清理，给协程一些时间完成
    m_cleanupTimer->start(500); // 500ms后开始清理
}

// 安全清理，确保不会重复清理
void PingWorker::safeCleanup()
{
    if (m_cleanupInProgress.load()) return;
    
    m_cleanupInProgress = true;
    emit logMessage("Starting safe cleanup...");
    
    // 停止所有定时器
    m_batchTimer->stop();
    m_stopCheckTimer->stop();
    
    // 启动清理定时器
    m_cleanupTimer->start(100);
}

// 真正的清理函数，释放资源并重置状态
void PingWorker::cleanup()
{
    if (!m_running.load()) return;
    
    emit logMessage("Cleaning up...");
    m_running = false;
    
    // 停止io_context，这会导致所有协程被取消
    if (m_workGuard) {
        m_workGuard.reset();
    }
    
    if (m_ioContext) {
        try {
            m_ioContext->stop();
        } catch (...) {
            // 忽略停止时的错误
        }
    }
    
    // 等待线程完成，使用更短的超时时间
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            // 尝试正常等待100ms
            auto start = std::chrono::steady_clock::now();
            while (thread.joinable() && 
                   std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // 如果还在运行，则分离线程
            if (thread.joinable()) {
                thread.detach();
            }
        }
    }
    m_threads.clear();
    
    // 清空队列
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<QString> empty;
        m_ipQueue.swap(empty);
    }
    
    // 重置io_context，准备下次使用
    try {
        m_ioContext = std::make_unique<boost::asio::io_context>();
        m_cancellationSignal = std::make_unique<boost::asio::cancellation_signal>();
    } catch (...) {
        emit logMessage("Error resetting io_context");
    }
    
    m_cleanupInProgress = false;
    emit finished();
}

// 处理下一批IP，批量调度ping任务
void PingWorker::processNextBatch()
{
    // 频繁检查停止状态
    if (m_stopRequested.load() || !m_running.load()) {
        return;
    }
    
    // 获取当前进度并检查是否已完成
    uint64_t processed = m_cidrExpander->getProcessedIPCount();
    uint64_t total = m_cidrExpander->getTotalIPCount();
    
    // 如果已经处理完所有IP，或者进度达到100%，停止任务
    if (processed >= total) {
        m_batchTimer->stop();
        QTimer::singleShot(100, this, [this]() {
            if (m_running.load() && !m_stopRequested.load()) {
                cleanup();
            }
        });
        return;
    }
    
    // 检查是否已处理完所有IP
    bool allIPsProcessed = !m_cidrExpander->hasMore();
    
    // 只要活跃任务数低于最大并发数且还有IP要处理，就持续添加新任务
    int currentActive = m_activePings.load();
    
    // 计算剩余未处理IP数
    uint64_t remainingIPs = total - processed;
    
    // 根据剩余IP数量和最大并发任务数确定可用槽位
    int availableSlots = std::min(m_maxConcurrentTasks - currentActive, 
                                static_cast<int>(remainingIPs));
    
    // 如果没有可用槽位，继续等待下一次定时器触发
    if (availableSlots <= 0) {
        // 如果所有IP已处理完且没有活跃任务，则可以结束了
        if (allIPsProcessed && currentActive == 0) {
            m_batchTimer->stop();
            QTimer::singleShot(100, this, [this]() {
                if (m_running.load() && !m_stopRequested.load()) {
                    cleanup();
                }
            });
        }
        return;
    }
    
    // 如果已经没有IP要处理了
    if (allIPsProcessed) {
        // 还有活跃任务，继续等待它们完成
        return;
    }
    
    // 确定本次要处理的IP数量
    int batchSize = std::min(availableSlots, BATCH_SIZE);
    QStringList ips = m_cidrExpander->getNextBatch(batchSize);
    
    for (const QString& ip : ips) {
        // 再次检查停止状态
        if (m_stopRequested.load()) {
            break;
        }
        
        // 增加活跃ping计数
        m_activePings++;
        
        // 预先转换IP为boost::asio::ip::address，避免在协程中进行字符串操作
        boost::system::error_code ec;
        auto address = boost::asio::ip::make_address(ip.toStdString(), ec);
        
        if (ec) {
            // IP地址无效，直接处理
            emit pingResult(ip, 0.0, false);
            if (m_enableLogging) {
                emit logMessage(QString("Invalid IP address: %1").arg(ip));
            }
            m_completedCount++;
            m_activePings--;
            continue;
        }
        
        // 将有效的地址和原始IP字符串一起传递给协程
        QString ipCopy = ip; // 复制IP字符串
        boost::asio::co_spawn(*m_ioContext, 
                             pingIPWithAddress(address, ipCopy), 
                             boost::asio::detached);
    }
    
    // 发送正确的进度信息
    processed = m_cidrExpander->getProcessedIPCount();
    total = m_cidrExpander->getTotalIPCount();
    
    // 确保total至少为processed，以避免显示奇怪的进度
    total = std::max(processed, total);
    
    emit progress(static_cast<int>(processed), static_cast<int>(total));
    
    // 如果进度达到100%且没有活跃任务，则停止任务
    if (processed >= total && m_activePings.load() == 0) {
        m_batchTimer->stop();
        QTimer::singleShot(100, this, [this]() {
            if (m_running.load() && !m_stopRequested.load()) {
                cleanup();
            }
        });
    }
}

// 单个IP的ping协程，负责连接并上报结果
boost::asio::awaitable<void> PingWorker::pingIPWithAddress(boost::asio::ip::address address, QString originalIP)
{
    try {
        // 早期检查停止状态
        if (m_stopRequested.load()) {
            m_activePings--;
            co_return;
        }
        
        auto executor = co_await boost::asio::this_coro::executor;
        boost::asio::ip::tcp::socket socket(executor);
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 创建端点，IPv6和IPv4都使用配置的端口号
        boost::asio::ip::tcp::endpoint endpoint(address, m_port);
       
        boost::asio::steady_timer timer(executor);
        timer.expires_after(std::chrono::milliseconds(std::min(m_timeoutMs, 2000)));
        
        using namespace boost::asio::experimental::awaitable_operators;

        try {
            // 在连接前再次检查停止状态
            if (m_stopRequested.load()) {
                m_activePings--;
                co_return;
            }
           

            // 并发等待连接或超时
            auto variant_result = co_await (
                socket.async_connect(endpoint, boost::asio::as_tuple(boost::asio::use_awaitable)) ||
                timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable))
            );
            std::size_t which = variant_result.index();
            boost::system::error_code ec2;
            if (which == 0) {
                ec2 = std::get<0>(std::get<0>(variant_result));
            } else {
                ec2 = std::get<0>(std::get<1>(variant_result));
            }
            auto end_time = std::chrono::steady_clock::now();
            double latency = IPUtils::calculateLatency(start_time, end_time);
            bool success = (which == 0 && !ec2);
            
            if (success) {
                boost::system::error_code close_ec;
                socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, close_ec);
                socket.close(close_ec);
            }
            
            if (!m_stopRequested.load()) {
                QMetaObject::invokeMethod(this, [this, originalIP, latency, success]() {
                    emit pingResult(originalIP, latency, success);
                    
                    if (m_enableLogging) {
                        if (success) {
                            // IPv6地址可能很长，使用适当的格式
                            QString protocol = originalIP.contains(':') ? "IPv6" : "IPv4";
                            emit logMessage(QString("TCP connect %1 (%2):%3: %4ms")
                                           .arg(originalIP).arg(protocol).arg(m_port).arg(latency, 0, 'f', 2));
                        } else {
                            emit logMessage(QString("TCP connect %1:%2 timeout").arg(originalIP).arg(m_port));
                        }
                    }
                }, Qt::QueuedConnection);
            }
            
        } catch (const boost::system::system_error& e) {
            if (m_stopRequested.load()) {
                m_activePings--;
                co_return;
            }
            
            auto end_time = std::chrono::steady_clock::now();
            double latency = IPUtils::calculateLatency(start_time, end_time);
            
            bool isReachable = (e.code() == boost::asio::error::connection_refused);
            
            // 忽略取消相关的错误
            if (e.code() != boost::asio::error::operation_aborted) {
                QMetaObject::invokeMethod(this, [this, originalIP, latency, isReachable]() {
                    emit pingResult(originalIP, latency, isReachable);
                    
                    if (m_enableLogging) {
                        QString protocol = originalIP.contains(':') ? "IPv6" : "IPv4";
                        if (isReachable) {
                            emit logMessage(QString("TCP connect %1 (%2):%3: %4ms (port closed but reachable)")
                                           .arg(originalIP).arg(protocol).arg(m_port).arg(latency, 0, 'f', 2));
                        } else {
                            emit logMessage(QString("TCP connect %1 (%2):%3 failed")
                                           .arg(originalIP).arg(protocol).arg(m_port));
                        }
                    }
                }, Qt::QueuedConnection);
            }
        } catch (const std::exception& e) {
            if (!m_stopRequested.load()) {
                QMetaObject::invokeMethod(this, [this, originalIP]() {
                    emit pingResult(originalIP, 0.0, false);
                    if (m_enableLogging) {
                        QString protocol = originalIP.contains(':') ? "IPv6" : "IPv4";
                        emit logMessage(QString("TCP connect %1 (%2):%3 failed with exception")
                                       .arg(originalIP).arg(protocol).arg(m_port));
                    }
                }, Qt::QueuedConnection);
            }
        }
        
    } catch (const std::exception& e) {
        if (!m_stopRequested.load()) {
            QMetaObject::invokeMethod(this, [this, originalIP]() {
                emit pingResult(originalIP, 0.0, false);
                if (m_enableLogging) {
                    QString protocol = originalIP.contains(':') ? "IPv6" : "IPv4";
                    emit logMessage(QString("TCP connect %1 (%2):%3 failed with exception")
                                   .arg(originalIP).arg(protocol).arg(m_port));
                }
            }, Qt::QueuedConnection);
        }
    }
    
    m_completedCount++;
    m_activePings--;
    
    // 检查是否已完成所有任务
    uint64_t processed = m_cidrExpander->getProcessedIPCount();
    uint64_t total = m_cidrExpander->getTotalIPCount();
    
    // 如果已经处理完所有IP，且没有活跃任务，则可以结束了
    if (processed >= total && m_activePings.load() == 0) {
        QMetaObject::invokeMethod(this, [this]() {
            if (m_running.load() && !m_stopRequested.load()) {
                m_batchTimer->stop();
                cleanup();
            }
        }, Qt::QueuedConnection);
    }
}