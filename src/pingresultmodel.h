#ifndef PINGRESULTMODEL_H
#define PINGRESULTMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QTimer>
#include <QColor>

struct PingResult {
    QString ip;
    double latency;
    bool success;
    
    PingResult(const QString& ip = "", double latency = 0.0, bool success = false)
        : ip(ip), latency(latency), success(success) {}
};

class PingResultModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PingResultModel(QObject *parent = nullptr);
    

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void addResult(const PingResult& result);
    void clear();
    QStringList getAllIPs() const;
    QStringList getSelectedIPs(const QModelIndexList& selection) const;
    
private slots:
    void processPendingUpdates();
    
private:
    void sortResults();
    
    QVector<PingResult> m_results;
    QVector<PingResult> m_pendingResults;
    QTimer* m_updateTimer;
    
    static constexpr int MAX_DISPLAY_COUNT = 100;
    static constexpr int UPDATE_INTERVAL_MS = 500; 
};

#endif // PINGRESULTMODEL_H
