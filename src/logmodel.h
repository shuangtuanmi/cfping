#ifndef LOGMODEL_H
#define LOGMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

struct LogEntry {
    QDateTime timestamp;
    QString message;
    
    LogEntry(const QString& msg = "")
        : timestamp(QDateTime::currentDateTime()), message(msg) {}
};

class LogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LogModel(QObject *parent = nullptr);
    

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void addLogMessage(const QString& message);
    void clear();
    
private slots:
    void processPendingUpdates();
    
private:
    QVector<LogEntry> m_logs;
    QVector<LogEntry> m_pendingLogs;
    mutable QMutex m_pendingMutex;
    QTimer* m_updateTimer;
    
    static constexpr int MAX_LOG_COUNT = 100;
    static constexpr int UPDATE_INTERVAL_MS = 200; // 每200ms更新一次
};

#endif // LOGMODEL_H
