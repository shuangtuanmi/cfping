#include "pingresultmodel.h"
#include <algorithm>

PingResultModel::PingResultModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_updateTimer(new QTimer(this))
{
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &PingResultModel::processPendingUpdates);
}

int PingResultModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_results.size();
}

int PingResultModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 3; // IP, 延迟, 状态
}

QVariant PingResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return QVariant();
    
    const PingResult& result = m_results[index.row()];
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return result.ip;
        case 1: return QString::number(result.latency, 'f', 2);
        case 2: return result.success ? "已连接" : "失败";
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == 1) { // 延迟列右对齐
            return Qt::AlignRight + Qt::AlignVCenter;
        }
        return Qt::AlignLeft + Qt::AlignVCenter;
    }
    else if (role == Qt::BackgroundRole) {
        // 为成功的连接设置淡绿色背景
        if (result.success) {
            return QColor(240, 255, 240);
        }
    }
    else if (role == Qt::ToolTipRole && index.column() == 0) {
        // 为IP列添加工具提示，特别对IPv6地址有用
        QString protocol = result.ip.contains(':') ? "IPv6" : "IPv4";
        return QString("%1 (%2)").arg(result.ip).arg(protocol);
    }
    
    return QVariant();
}

QVariant PingResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return "IP地址 (IPv4/IPv6)";
        case 1: return "延迟 (毫秒)";
        case 2: return "状态";
        }
    }
    return QVariant();
}

void PingResultModel::addResult(const PingResult& result)
{
    // 只添加成功的结果到待处理队列
    if (result.success) {
        m_pendingResults.append(result);
        
        // 启动定时器（如果还没启动）
        if (!m_updateTimer->isActive()) {
            m_updateTimer->start();
        }
    }
}

void PingResultModel::clear()
{
    beginResetModel();
    m_results.clear();
    m_pendingResults.clear();
    m_updateTimer->stop();
    endResetModel();
}

void PingResultModel::processPendingUpdates()
{
    if (m_pendingResults.isEmpty()) {
        m_updateTimer->stop();
        return;
    }
    
    // 批量添加结果
    int oldSize = m_results.size();
    m_results.append(m_pendingResults);
    m_pendingResults.clear();
    
    // 限制显示数量并排序
    if (m_results.size() > MAX_DISPLAY_COUNT * 2) {
        sortResults();
        if (m_results.size() > MAX_DISPLAY_COUNT) {
            m_results.resize(MAX_DISPLAY_COUNT);
        }
        
        // 数据结构发生重大变化，重置模型
        beginResetModel();
        endResetModel();
    } else {
        // 简单添加，通知视图
        if (m_results.size() > oldSize) {
            beginInsertRows(QModelIndex(), oldSize, m_results.size() - 1);
            endInsertRows();
            
            // 对新数据进行排序
            sortResults();
            beginResetModel();
            endResetModel();
        }
    }
}

void PingResultModel::sortResults()
{
    std::sort(m_results.begin(), m_results.end(), 
             [](const PingResult& a, const PingResult& b) {
                 if (a.success != b.success) return a.success > b.success;
                 if (!a.success) return false;
                 return a.latency < b.latency;
             });
}

QStringList PingResultModel::getAllIPs() const
{
    QStringList ips;
    for (const auto& result : m_results) {
        if (result.success) {
            ips.append(result.ip);
        }
    }
    return ips;
}

QStringList PingResultModel::getSelectedIPs(const QModelIndexList& selection) const
{
    QStringList ips;
    for (const auto& index : selection) {
        if (index.isValid() && index.column() == 0 && index.row() < m_results.size()) {
            const PingResult& result = m_results[index.row()];
            if (result.success) {
                ips.append(result.ip);
            }
        }
    }
    return ips;
}
