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
#include <QtGui/QClipboard>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>
#include <memory>

class PingWorker;
class PingResultModel;
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
    
    // UI Components
    QWidget* m_centralWidget;
    QSplitter* m_mainSplitter;
    QSplitter* m_rightSplitter;
    
    // Left panel - CIDR input
    QTextEdit* m_cidrTextEdit;
    QPushButton* m_openFileButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QPushButton* m_saveButton;
    
    // Settings
    QSpinBox* m_threadCountSpinBox;
    QSpinBox* m_timeoutSpinBox;
    QCheckBox* m_enableLoggingCheckBox;
    
    // Right panel - Results and logs
    QTableView* m_resultsTable;
    PingResultModel* m_resultsModel;
    QPlainTextEdit* m_logTextEdit;
    QPushButton* m_copyButton;
    
    // Status
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_testCountLabel;
    
    // Worker and data
    std::unique_ptr<PingWorker> m_pingWorker;
    QThread* m_workerThread;
    QTimer* m_updateTimer;
    
    bool m_isRunning;
    int m_totalIPs;
    int m_completedIPs;
};

#endif // MAINWINDOW_H
