#ifndef CHARTDATAWORKER_H
#define CHARTDATAWORKER_H

#include <QObject>
#include <QSqlDatabase>
#include <QMutex>
#include <QMap>
#include <QVector>
#include <QPointF>

struct FieldData
{
    QString fieldName;
    QVector<QPointF> dataPoints;
    float yMin = 0;
    float yMax = 0;
};

class ChartDataWorker : public QObject
{
    Q_OBJECT
public:
    explicit ChartDataWorker(const QString& dbFilePath, QObject* parent = nullptr);
    ~ChartDataWorker();

    static void clearConnectionPool();

public slots:
    void loadSpecifiedFields(int loadMin, int loadMax, const QStringList& fields); //指定字段

signals:
    void fieldDataLoaded(const FieldData& data);
    void progressUpdated(int percent);
    void allDataLoaded(float globalYMin, float globalYMax);
    void errorOccurred(const QString& msg);

private:
    QString m_dbFilePath;
    static QMutex s_connPoolMutex;
    static QMap<QString, QSqlDatabase> s_connPool;
};

#endif