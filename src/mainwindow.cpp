#include "mainwindow.h"
#include "pingworker.h"
#include "pingresultmodel.h"
#include "logmodel.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QClipboard>
#include <QtCore/QTextStream>
#include <QtCore/QDir>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_pingWorker(nullptr)
    , m_workerThread(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_isRunning(false)
    , m_totalIPs(0)
    , m_completedIPs(0)
{
    setupUI();
    setupConnections();
    m_updateTimer->setInterval(1000);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateResultsDisplay);   
    enableControls(true);
    addLogMessage("应用程序已启动。请加载CIDR地址段开始测试。");
}

MainWindow::~MainWindow()
{
    if (m_isRunning) {
        stopPing();
    }
    
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    // 主分割器（左侧面板 | 右侧面板）
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧面板 - CIDR输入和控制
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    
    // CIDR输入
    leftLayout->addWidget(new QLabel("CIDR地址段:"));
    m_cidrTextEdit = new QTextEdit();
    m_cidrTextEdit->setPlainText("# 输入CIDR地址段 (每行一个)\n# IPv4示例:\n104.16.0.0/13\n104.24.0.0/14\n108.162.192.0/18");
    leftLayout->addWidget(m_cidrTextEdit);
    
    // 文件操作
    QHBoxLayout* fileLayout = new QHBoxLayout();
    m_openFileButton = new QPushButton("打开文件");
    fileLayout->addWidget(m_openFileButton);
    fileLayout->addStretch();
    leftLayout->addLayout(fileLayout);
    
    // 设置区域
    QGridLayout* settingsLayout = new QGridLayout();
    settingsLayout->addWidget(new QLabel("线程数量:"), 0, 0);
    m_threadCountSpinBox = new QSpinBox();
    m_threadCountSpinBox->setRange(1, 16);
    m_threadCountSpinBox->setValue(4);
    settingsLayout->addWidget(m_threadCountSpinBox, 0, 1);
    
    settingsLayout->addWidget(new QLabel("超时时间 (毫秒):"), 1, 0);
    m_timeoutSpinBox = new QSpinBox();
    m_timeoutSpinBox->setRange(10, 5000);
    m_timeoutSpinBox->setValue(500);
    settingsLayout->addWidget(m_timeoutSpinBox, 1, 1);
    
    settingsLayout->addWidget(new QLabel("最大并发任务:"), 2, 0);
    m_concurrentTasksSpinBox = new QSpinBox();
    m_concurrentTasksSpinBox->setRange(10, 10000);
    m_concurrentTasksSpinBox->setValue(500);
    m_concurrentTasksSpinBox->setSingleStep(10);
    settingsLayout->addWidget(m_concurrentTasksSpinBox, 2, 1);

    settingsLayout->addWidget(new QLabel("端口号:"), 3, 0);
    m_portSpinBox = new QSpinBox();
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(80);
    settingsLayout->addWidget(m_portSpinBox, 3, 1);

    m_enableLoggingCheckBox = new QCheckBox("启用详细日志");
    settingsLayout->addWidget(m_enableLoggingCheckBox, 4, 0, 1, 2);
    
    leftLayout->addLayout(settingsLayout);
    
    // 控制按钮
    QHBoxLayout* controlLayout = new QHBoxLayout();
    m_startButton = new QPushButton("开始测试");
    m_stopButton = new QPushButton("停止");
    m_saveButton = new QPushButton("保存结果");
    
    controlLayout->addWidget(m_startButton);
    controlLayout->addWidget(m_stopButton);
    controlLayout->addWidget(m_saveButton);
    
    leftLayout->addLayout(controlLayout);
    
    // 状态显示
    m_progressBar = new QProgressBar();
    m_statusLabel = new QLabel("就绪");
    m_testCountLabel = new QLabel("IP地址: 0 / 0");
    m_elapsedTimeLabel = new QLabel("已耗时: 00:00:00");
    m_remainingTimeLabel = new QLabel("剩余时间: --:--:--");
    m_estimatedFinishLabel = new QLabel("预计完成: --:--:--");

    leftLayout->addWidget(m_progressBar);
    leftLayout->addWidget(m_statusLabel);
    leftLayout->addWidget(m_testCountLabel);
    leftLayout->addWidget(m_elapsedTimeLabel);
    leftLayout->addWidget(m_remainingTimeLabel);
    leftLayout->addWidget(m_estimatedFinishLabel);
    
    leftLayout->addStretch();
    leftPanel->setMaximumWidth(350);
    m_mainSplitter->addWidget(leftPanel);
    
    // 右侧面板 - 结果和日志
    m_rightSplitter = new QSplitter(Qt::Vertical);
    
    // 结果表格及模型
    QWidget* resultsWidget = new QWidget();
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsWidget);
    
    QHBoxLayout* resultsHeaderLayout = new QHBoxLayout();
    resultsHeaderLayout->addWidget(new QLabel("最快IP地址 (前100个):"));
    resultsHeaderLayout->addStretch();
    m_copyButton = new QPushButton("复制选中IP");
    resultsHeaderLayout->addWidget(m_copyButton);
    resultsLayout->addLayout(resultsHeaderLayout);
    
    // 使用QTableView和自定义模型
    m_resultsModel = new PingResultModel(this);
    m_resultsTable = new QTableView();
    m_resultsTable->setModel(m_resultsModel);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSortingEnabled(false); // 禁用列排序，由我们自己处理
    m_resultsTable->setShowGrid(false);
    
    // 设置列宽
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->horizontalHeader()->resizeSection(0, 120); // IP列
    m_resultsTable->horizontalHeader()->resizeSection(1, 100); // 延迟列
    
    resultsLayout->addWidget(m_resultsTable);
    
    // 日志 - 改为使用 TableView 和模型
    QWidget* logsWidget = new QWidget();
    QVBoxLayout* logsLayout = new QVBoxLayout(logsWidget);
    logsLayout->addWidget(new QLabel("日志 (最近100条):"));
    
    m_logModel = new LogModel(this);
    m_logTable = new QTableView();
    m_logTable->setModel(m_logModel);
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->setAlternatingRowColors(true);
    m_logTable->setSortingEnabled(false);
    m_logTable->setShowGrid(false);
    m_logTable->verticalHeader()->setVisible(false);
    
    // 设置日志表格列宽
    m_logTable->horizontalHeader()->resizeSection(0, 80);  // 时间列
    m_logTable->horizontalHeader()->setStretchLastSection(true); // 消息列自动拉伸
    
    // 自动滚动到最新日志
    connect(m_logModel, &LogModel::rowsInserted, this, [this]() {
        m_logTable->scrollToBottom();
    });
    
    logsLayout->addWidget(m_logTable);
    
    m_rightSplitter->addWidget(resultsWidget);
    m_rightSplitter->addWidget(logsWidget);
    m_rightSplitter->setSizes({400, 200});
    
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setSizes({350, 650});
    
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->addWidget(m_mainSplitter);
    
    setWindowTitle("CloudFlare CDN IP TCP连接测试工具");
    setMinimumSize(1000, 700);
    resize(1200, 800);
}

void MainWindow::setupConnections()
{
    connect(m_openFileButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startPing);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopPing);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveResults);
    connect(m_copyButton, &QPushButton::clicked, this, &MainWindow::copySelectedIPs);
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "打开CIDR文件", "", "文本文件 (*.txt);;所有文件 (*)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            m_cidrTextEdit->setPlainText(in.readAll());
            addLogMessage(QString("已加载CIDR文件: %1").arg(fileName));
        } else {
            QMessageBox::warning(this, "错误", "无法打开文件。");
        }
    }
}

void MainWindow::startPing()
{
    if (m_isRunning) return;
    
    QString cidrText = m_cidrTextEdit->toPlainText();
    QStringList cidrRanges;
    
    for (const QString& line : cidrText.split('\n')) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith('#')) {
            cidrRanges.append(trimmed);
        }
    }
    
    if (cidrRanges.isEmpty()) {
        QMessageBox::warning(this, "警告", "请至少输入一个CIDR地址段。");
        return;
    }
    
    m_isRunning = true;
    m_completedIPs = 0;
    m_totalIPs = 0;
    m_startTime = QDateTime::currentDateTime();  // 记录开始时间

    // 清空结果模型
    m_resultsModel->clear();

    // 清空日志
    m_logModel->clear();
    
    // 创建工作线程
    try {
        m_pingWorker = std::make_unique<PingWorker>();
        m_workerThread = new QThread(this);
        m_pingWorker->moveToThread(m_workerThread);
        
        // 使用临时变量存储设置，避免lambda捕获this
        int threadCount = m_threadCountSpinBox->value();
        int timeout = m_timeoutSpinBox->value();
        int maxConcurrentTasks = m_concurrentTasksSpinBox->value();
        int port = m_portSpinBox->value();  // 获取端口号
        bool enableLogging = m_enableLoggingCheckBox->isChecked();
        QStringList ranges = cidrRanges;

        // 连接信号
        connect(m_workerThread, &QThread::started, [this, threadCount, timeout, enableLogging, maxConcurrentTasks, port, ranges]() {
            if (m_pingWorker) {
                m_pingWorker->setSettings(threadCount, timeout, enableLogging, maxConcurrentTasks, port);
                m_pingWorker->startPing(ranges);
            }
        });
        
        // 使用Qt::QueuedConnection确保信号在主线程中处理
        connect(m_pingWorker.get(), &PingWorker::pingResult, this, &MainWindow::onPingResult, Qt::QueuedConnection);
        connect(m_pingWorker.get(), &PingWorker::progress, this, &MainWindow::onPingProgress, Qt::QueuedConnection);
        connect(m_pingWorker.get(), &PingWorker::logMessage, this, &MainWindow::onPingLog, Qt::QueuedConnection);
        connect(m_pingWorker.get(), &PingWorker::finished, this, &MainWindow::onPingFinished, Qt::QueuedConnection);
        
        // 更新UI状态
        enableControls(false);
        m_updateTimer->start();
        
        // 启动线程
        m_workerThread->start();
        
        addLogMessage(QString("开始TCP连接测试 (端口%1)...").arg(port));
    }
    catch (const std::exception& e) {
        addLogMessage(QString("启动测试时出错: %1").arg(e.what()));
        m_isRunning = false;
        enableControls(true);
    }
}

void MainWindow::stopPing()
{
    if (!m_isRunning) return;
    
    // 立即更新UI状态，让用户知道停止请求已收到
    m_statusLabel->setText("正在停止...");
    m_stopButton->setEnabled(false);
    addLogMessage("收到停止请求，正在停止测试...");
    
    // 立即停止UI更新定时器
    m_updateTimer->stop();
    
    // 通知工作线程停止
    if (m_pingWorker) {
        QMetaObject::invokeMethod(m_pingWorker.get(), "stopPing", Qt::QueuedConnection);
    }
    
    // 设置更长的超时时间，给协程足够时间清理
    QTimer::singleShot(3000, this, [this]() {
        if (m_isRunning) {
            addLogMessage("强制停止测试...");
            
            if (m_workerThread && m_workerThread->isRunning()) {
                // 断开信号连接防止崩溃
                if (m_pingWorker) {
                    m_pingWorker->disconnect();
                }
                
                // 强制终止线程
                m_workerThread->terminate();
                m_workerThread->wait(1000);
            }
            
            // 强制恢复UI状态
            onPingFinished();
        }
    });
    
    // 立即处理挂起的事件，确保UI响应
    QCoreApplication::processEvents();
}

void MainWindow::saveResults()
{
    QStringList allIPs = m_resultsModel->getAllIPs();
    
    if (allIPs.isEmpty()) {
        QMessageBox::information(this, "信息", "没有结果可保存。");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, 
        "保存结果", "tcp_test_results.txt", "文本文件 (*.txt)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "# CloudFlare CDN IP TCP连接测试结果\n";
            out << "# 按延迟排序的成功连接IP地址\n";
            
            for (const QString& ip : allIPs) {
                out << ip << "\n";
            }
            
            addLogMessage(QString("结果已保存到: %1 (%2个IP)").arg(fileName).arg(allIPs.size()));
        } else {
            QMessageBox::warning(this, "错误", "无法保存文件。");
        }
    }
}

void MainWindow::onPingResult(const QString& ip, double latency, bool success)
{
    // 直接添加到模型，模型会处理批量更新
    m_resultsModel->addResult(PingResult(ip, latency, success));
    m_completedIPs++;
}

void MainWindow::onPingProgress(int current, int total)
{
    // 简化进度更新，减少UI阻塞
    m_completedIPs = current;
    m_totalIPs = total;
    
    // 确保进度值不超过100%
    if (m_completedIPs > m_totalIPs) {
        m_completedIPs = m_totalIPs;
    }
    
    // 更新UI显示
    updateResultsDisplay();
    
    // 每1000个IP更新一次UI，减少更新频率
    if (current % 1000 == 0) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
    }
    
    // 如果已完成所有IP，检查是否需要停止任务
    if (m_isRunning && m_completedIPs >= m_totalIPs) {
        // 添加计时器检查，避免过早停止
        QTimer::singleShot(2000, this, [this]() {
            if (m_isRunning && m_completedIPs >= m_totalIPs) {
                // 如果仍在运行且已全部完成，则停止任务
                stopPing();
            }
        });
    }
}

void MainWindow::onPingLog(const QString& message)
{
    addLogMessage(message);
}

void MainWindow::onPingFinished()
{
    m_isRunning = false;
    m_updateTimer->stop();
    
    // 立即恢复控件状态
    enableControls(true);
    updateResultsDisplay();
    addLogMessage("测试已完成。");
    
    m_statusLabel->setText("已完成");
    
    // 异步清理线程资源，给更多时间
    QTimer::singleShot(1000, this, [this]() {
        if (m_workerThread) {
            if (m_workerThread->isRunning()) {
                m_workerThread->quit();
                if (!m_workerThread->wait(2000)) {
                    m_workerThread->terminate();
                    m_workerThread->wait(1000);
                }
            }
            m_workerThread->deleteLater();
            m_workerThread = nullptr;
        }
        m_pingWorker.reset();
    });
}

void MainWindow::updateResultsDisplay()
{
    // 更新进度信息
    if (m_totalIPs > 0) {
        // 确保百分比不超过100%
        int percentage = std::min(100, (m_completedIPs * 100) / m_totalIPs);
        m_progressBar->setValue(percentage);
        m_testCountLabel->setText(QString("IP地址: %1 / %2").arg(m_completedIPs).arg(m_totalIPs));

        // 计算时间信息
        if (m_isRunning && m_startTime.isValid()) {
            QDateTime currentTime = QDateTime::currentDateTime();
            qint64 elapsedMs = m_startTime.msecsTo(currentTime);

            // 格式化已耗时
            int elapsedSeconds = elapsedMs / 1000;
            int hours = elapsedSeconds / 3600;
            int minutes = (elapsedSeconds % 3600) / 60;
            int seconds = elapsedSeconds % 60;
            m_elapsedTimeLabel->setText(QString("已耗时: %1:%2:%3")
                .arg(hours, 2, 10, QChar('0'))
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0')));

            // 计算剩余时间和预计完成时间
            if (m_completedIPs > 0 && m_completedIPs < m_totalIPs) {
                // 计算平均每个IP的处理时间
                double avgTimePerIP = static_cast<double>(elapsedMs) / m_completedIPs;
                int remainingIPs = m_totalIPs - m_completedIPs;
                qint64 estimatedRemainingMs = static_cast<qint64>(avgTimePerIP * remainingIPs);

                // 格式化剩余时间
                int remainingSeconds = estimatedRemainingMs / 1000;
                int remHours = remainingSeconds / 3600;
                int remMinutes = (remainingSeconds % 3600) / 60;
                int remSecs = remainingSeconds % 60;
                m_remainingTimeLabel->setText(QString("剩余时间: %1:%2:%3")
                    .arg(remHours, 2, 10, QChar('0'))
                    .arg(remMinutes, 2, 10, QChar('0'))
                    .arg(remSecs, 2, 10, QChar('0')));

                // 计算预计完成时间
                QDateTime estimatedFinish = currentTime.addMSecs(estimatedRemainingMs);
                m_estimatedFinishLabel->setText(QString("预计完成: %1")
                    .arg(estimatedFinish.toString("hh:mm:ss")));
            } else if (m_completedIPs >= m_totalIPs) {
                m_remainingTimeLabel->setText("剩余时间: 00:00:00");
                m_estimatedFinishLabel->setText("预计完成: 已完成");
            }
        }

        // 在进度条100%时更新状态文本
        if (percentage >= 100) {
            m_statusLabel->setText("测试完成");
        }
    }
}

void MainWindow::copySelectedIPs()
{
    QModelIndexList selectedIndexes = m_resultsTable->selectionModel()->selectedRows();
    QStringList selectedIPs = m_resultsModel->getSelectedIPs(selectedIndexes);
    
    if (selectedIPs.isEmpty()) {
        // 如果没有选中，则复制所有可见IP
        selectedIPs = m_resultsModel->getAllIPs();
    }
    
    if (!selectedIPs.isEmpty()) {
        QApplication::clipboard()->setText(selectedIPs.join('\n'));
        addLogMessage(QString("已复制 %1 个IP地址到剪贴板。").arg(selectedIPs.size()));
    }
}

void MainWindow::enableControls(bool enabled)
{
    m_startButton->setEnabled(enabled);
    m_stopButton->setEnabled(!enabled);
    m_openFileButton->setEnabled(enabled);
    m_threadCountSpinBox->setEnabled(enabled);
    m_timeoutSpinBox->setEnabled(enabled);
    m_concurrentTasksSpinBox->setEnabled(enabled);
    m_portSpinBox->setEnabled(enabled);  // 添加端口号控件的启用/禁用
    m_enableLoggingCheckBox->setEnabled(enabled);

    if (enabled) {
        m_statusLabel->setText("就绪");
        m_progressBar->setValue(0);
        // 重置时间显示
        m_elapsedTimeLabel->setText("已耗时: 00:00:00");
        m_remainingTimeLabel->setText("剩余时间: --:--:--");
        m_estimatedFinishLabel->setText("预计完成: --:--:--");
    } else {
        m_statusLabel->setText("正在运行...");
    }
}

void MainWindow::addLogMessage(const QString& message)
{
    if (m_logModel) {
        m_logModel->addLogMessage(message);
    }
}
