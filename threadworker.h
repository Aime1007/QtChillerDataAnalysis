#ifndef THREADWORKER_H
#define THREADWORKER_H

#include <QObject>
#include <QStringList>
#include <QList>

// 子线程工作类（处理Excel读取+数据整理，不直接操作数据库）
class ThreadWorker : public QObject
{
    Q_OBJECT
public:
    explicit ThreadWorker(QObject* parent = nullptr);

signals:
    // 进度更新信号（可选，用于显示导入进度）
    void progressUpdated(int progress);
    // 完成信号（传递是否成功、提示信息、解析后的表头+数据）
    void finished(bool success, const QString& msg, const QStringList& headers, const QList<QStringList>& data);

public slots:
    // 导入Excel的核心槽函数（子线程中执行）
    void doImportXlsx(const QString& filePath);

private:
    // 读取Excel（和原逻辑一致，移到这里）
    bool readXlsx(const QString& filePath, QStringList& headers, QList<QStringList>& data);
};

#endif // THREADWORKER_H