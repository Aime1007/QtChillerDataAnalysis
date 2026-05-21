#include "ChartDataWorker.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QThread>

QMutex ChartDataWorker::s_connPoolMutex;
QMap<QString, QSqlDatabase> ChartDataWorker::s_connPool;

ChartDataWorker::ChartDataWorker(const QString& dbFilePath, QObject* parent)
    : QObject(parent), m_dbFilePath(dbFilePath)
{
}

ChartDataWorker::~ChartDataWorker()
{
}

void ChartDataWorker::clearConnectionPool()
{
    QMutexLocker locker(&s_connPoolMutex);
    for (QSqlDatabase db : s_connPool.values()) {
        if (db.isOpen()) db.close();
    }
    s_connPool.clear();
}

void ChartDataWorker::loadSpecifiedFields(int loadMin, int loadMax, const QStringList& fields)
{
    QElapsedTimer timer;
    timer.start();
    int totalRows = loadMax - loadMin + 1;

    QMutexLocker locker(&s_connPoolMutex);
    QSqlDatabase db;

    if (s_connPool.contains(m_dbFilePath)) {
        db = s_connPool[m_dbFilePath];
    }
    else {
        QString connName = QString("ChartWorker_%1").arg(timer.elapsed());
        db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(m_dbFilePath);
        db.setConnectOptions(
            "QSQLITE_OPEN_READONLY;"
            "QSQLITE_BUSY_TIMEOUT=5000;"
        );

        if (!db.open()) {
            emit errorOccurred("打开数据库失败");
            return;
        }
        s_connPool[m_dbFilePath] = db;
    }
    locker.unlock();

    QString sql = QString("SELECT %1 FROM excel_data LIMIT :limit OFFSET :offset")
        .arg(fields.join(","));

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":limit", totalRows);
    query.bindValue(":offset", loadMin);

    if (!query.exec()) {
        emit errorOccurred("查询失败：" + query.lastError().text());
        return;
    }

    QMap<QString, FieldData> fieldMap;
    for (const QString& f : fields) {
        FieldData fd;
        fd.fieldName = f;
        fd.dataPoints.reserve(totalRows);
        fieldMap[f] = fd;
    }

    int currentRow = 0;
    float globalMin = 0, globalMax = 0;
    bool first = true;

    while (query.next()) {
        for (int i = 0; i < fields.size(); ++i) {
            QString field = fields[i];
            FieldData& fd = fieldMap[field];

            bool ok;
            float v = query.value(i).toFloat(&ok);
            if (!ok || qIsNaN(v) || qIsInf(v)) continue;

            fd.dataPoints.append(QPointF(loadMin + currentRow, v));

            if (fd.dataPoints.size() == 1) {
                fd.yMin = fd.yMax = v;
            }
            else {
                fd.yMin = qMin(fd.yMin, v);
                fd.yMax = qMax(fd.yMax, v);
            }

            if (first) {
                globalMin = globalMax = v;
                first = false;
            }
            else {
                globalMin = qMin(globalMin, v);
                globalMax = qMax(globalMax, v);
            }
        }

        currentRow++;
        if (currentRow % 500 == 0)
            emit progressUpdated((currentRow * 100) / totalRows);
    }

    for (const QString& f : fields)
        emit fieldDataLoaded(fieldMap[f]);

    emit allDataLoaded(globalMin, globalMax);
}