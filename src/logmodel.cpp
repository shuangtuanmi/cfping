#include "logmodel.h"
#include <QCoreApplication>

LogModel::LogModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_updateTimer(new QTimer(this))
{
    // 确保定时器在主线程中创建和运行
    m_updateTimer->moveToThread(QCoreApplication::instance()->thread());
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    
    // 使用Qt::QueuedConnection确保跨线程信号安全
    connect(m_updateTimer, &QTimer::timeout, this, &LogModel::processPendingUpdates, Qt::QueuedConnection);
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_logs.size();
}

int LogModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 2; // 时间, 消息
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_logs.size())
        return QVariant();
    
    const LogEntry& entry = m_logs[index.row()];
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return entry.timestamp.toString("hh:mm:ss");
        case 1: return entry.message;
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == 0) { // 时间列居中对齐
            return Qt::AlignCenter;
        }
        return Qt::AlignLeft + Qt::AlignVCenter;
    }
    
    return QVariant();
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return "时间";
        case 1: return "日志消息";
        }
    }
    return QVariant();
}

void LogModel::addLogMessage(const QString& message)
{
    // 线程安全地添加消息
    QMutexLocker locker(&m_pendingMutex);
    m_pendingLogs.append(LogEntry(message));
    
    // 使用Qt::QueuedConnection确保在主线程中启动定时器
    if (!m_updateTimer->isActive()) {
        QMetaObject::invokeMethod(m_updateTimer, "start", Qt::QueuedConnection);
    }
}

void LogModel::clear()
{
    beginResetModel();
    m_logs.clear();
    
    // 线程安全地清空待处理日志
    {
        QMutexLocker locker(&m_pendingMutex);
        m_pendingLogs.clear();
    }
    
    // 安全停止定时器
    QMetaObject::invokeMethod(m_updateTimer, "stop", Qt::QueuedConnection);
    endResetModel();
}

void LogModel::processPendingUpdates()
{
    QVector<LogEntry> newLogs;
    
    // 线程安全地获取待处理日志
    {
        QMutexLocker locker(&m_pendingMutex);
        if (m_pendingLogs.isEmpty()) {
            m_updateTimer->stop();
            return;
        }
        newLogs = m_pendingLogs;
        m_pendingLogs.clear();
    }
    
    // 批量添加日志
    int oldSize = m_logs.size();
    m_logs.append(newLogs);
    
    // 限制日志数量，保留最新的日志
    if (m_logs.size() > MAX_LOG_COUNT) {
        int removeCount = m_logs.size() - MAX_LOG_COUNT;
        m_logs.remove(0, removeCount);
        
        // 数据结构发生重大变化，重置模型
        beginResetModel();
        endResetModel();
    } else {
        // 简单添加，通知视图
        if (m_logs.size() > oldSize) {
            beginInsertRows(QModelIndex(), oldSize, m_logs.size() - 1);
            endInsertRows();
            
            // 自动滚动到最新日志
            QTimer::singleShot(0, this, [this]() {
                emit dataChanged(createIndex(m_logs.size() - 1, 0), createIndex(m_logs.size() - 1, 1));
            });
        }
    }
}
