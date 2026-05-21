#include "threadworker.h"
#include <xlsxdocument.h>
#include <QDebug>

ThreadWorker::ThreadWorker(QObject* parent) : QObject(parent)
{
}

void ThreadWorker::doImportXlsx(const QString& filePath)
{
    QStringList headers;
    QList<QStringList> data;

    // 1. 读取Excel（子线程中执行，不阻塞主线程）
    if (!readXlsx(filePath, headers, data)) {
        emit finished(false, "读取Excel文件失败", headers, data);
        return;
    }

    // 2. 读取成功，传递数据给主线程（数据库插入在主线程做，避免跨线程操作数据库）
    emit finished(true, QString("成功读取%1行数据").arg(data.size()), headers, data);
}

bool ThreadWorker::readXlsx(const QString& filePath, QStringList& headers, QList<QStringList>& data)
{
    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        qDebug() << "Excel文件读取失败：" << filePath;
        return false;
    }

    // 获取Excel有效范围
    QXlsx::CellRange range = xlsx.dimension();
    if (!range.isValid()) {
        qDebug() << "Excel无有效数据";
        return false;
    }

    // 读取表头
    for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
        auto cell = xlsx.cellAt(range.firstRow(), col);
        headers.append(cell ? cell->value().toString().trimmed() : "空列");
    }

    // 检测表头是否全为数字（预处理数据常见问题），若是则生成有意义的列名
    bool allNumeric = !headers.isEmpty();
    for (const QString& h : headers) {
        bool ok;
        h.toDouble(&ok);
        if (!ok) { allNumeric = false; break; }
    }
    if (allNumeric) {
        qDebug() << "[WARN] 检测到纯数字表头，自动生成列名";
        for (int i = 0; i < headers.size(); ++i) {
            headers[i] = QString("特征%1").arg(i + 1);
        }
    }

    // 读取数据行（带进度更新）
    int totalRows = range.lastRow() - range.firstRow();
    for (int row = range.firstRow() + 1; row <= range.lastRow(); ++row) {
        QStringList rowData;
        for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
            auto cell = xlsx.cellAt(row, col);
            QString cellValue = cell ? cell->value().toString().trimmed() : "";
            rowData.append(cellValue);
        }
        if (!rowData.isEmpty()) {
            data.append(rowData);
        }
        // 发送进度（可选）
        int progress = ((row - range.firstRow()) * 100) / totalRows;
        emit progressUpdated(progress);
    }

    qDebug() << "表头：" << headers;
    qDebug() << "数据行数：" << data.size();
    return true;
}