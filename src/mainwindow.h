#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QApplication>
#include <QClipboard>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QCoreApplication>
#include <memory>

class PingWorker;
class PingResultModel;
class LogModel;
struct PingResult;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void startPing();
    void stopPing();
    void saveResults();
    void onPingResult(const QString& ip, double latency, bool success);
    void onPingProgress(int current, int total);
    void onPingLog(const QString& message);
    void onPingFinished();
    void updateResultsDisplay();
    void copySelectedIPs();

private:
    void setupUI();
    void setupConnections();
    void enableControls(bool enabled);
    void addLogMessage(const QString& message);
    
    // UI组件
    QWidget* m_centralWidget;
    QSplitter* m_mainSplitter;
    QSplitter* m_rightSplitter;
    
    // 左侧面板 - CIDR输入
    QTextEdit* m_cidrTextEdit;
    QPushButton* m_openFileButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_saveButton;
    
    // 设置区域
    QSpinBox* m_threadCountSpinBox;
    QSpinBox* m_timeoutSpinBox;
    QSpinBox* m_concurrentTasksSpinBox;  //最大并发任务控制
    QSpinBox* m_portSpinBox;  // 端口号配置
    QCheckBox* m_enableLoggingCheckBox;
    
    // 右侧面板 - 结果和日志
    QTableView* m_resultsTable;
    PingResultModel* m_resultsModel;
    QTableView* m_logTable;  
    LogModel* m_logModel;    //日志模型
    QPushButton* m_copyButton;
    
    // 状态显示
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_testCountLabel;
    QLabel* m_elapsedTimeLabel;      // 已耗时显示
    QLabel* m_remainingTimeLabel;    // 剩余时间显示
    QLabel* m_estimatedFinishLabel;  // 预计完成时间显示
    
    // 工作线程与数据
    std::unique_ptr<PingWorker> m_pingWorker;
    QThread* m_workerThread;
    QTimer* m_updateTimer;
    
    bool m_isRunning;
    int m_totalIPs;
    int m_completedIPs;
    QDateTime m_startTime;  // 开始时间记录
};

#endif // MAINWINDOW_H
