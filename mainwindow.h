#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QThread>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QSqlTableModel>
#include <QChart> // 添加图表库
#include <QLineSeries> // 添加线系列表库
#include <QValueAxis> // 添加值轴库
#include <QSqlError>
#include <QTimer>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>  // Qt6 中控制声音需要这个
#include <QUrl>
#include <QVBoxLayout>
#include <QProcess>
#include "threadworker.h"
#include "ChartDataWorker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

signals:
    void startLoadSpecifiedFields(int loadMin, int loadMax, const QStringList& fields); // 触发子线程加载指定字段数据
private slots:

    void onWorkerFinished(bool success, const QString& msg, const QStringList& headers, const QList<QStringList>& data);
    void onProgressUpdated(int progress); // 更新进度条
    void on_btn_browse_clicked(); // 浏览文件

    //主表格
    void loadDataToTable(); // 加载数据到表格
    void on_cbx_page_size_currentIndexChanged(int index);
    void on_btn_prev_page_clicked();
    void on_btn_next_page_clicked();
    void on_btn_jump_page_clicked();
    void on_btn_upload_clicked();
    void on_le_search_textChanged(const QString& arg1);
    // 数据库表格
    void on_le_search_2_textChanged(const QString& arg1);
    void on_cbx_page_size_2_currentIndexChanged(int index);
    void on_btn_prev_page_2_clicked();
    void on_btn_next_page_2_clicked();
    void on_btn_jump_page_2_clicked();

    void refreshChart(); // 刷新图表
    void onVarCheckBoxClicked(bool checked); // 勾选框点击事件
    void onSliderRangeChanged(int min, int max); // 滑动条范围变化事件
    void on_btn_show_chart_clicked();

    //工作线程
    void onFieldDataLoaded(const FieldData& data);
    void onAllDataLoaded(float globalMin, float globalMax);
    void onErrorOccurred(QString errorMsg);
    void on_btn_save_chart_clicked();

    void on_btn_export_data_clicked();

    void onRealtimeTimeout(); // 定时器触发时的执行函数
    void on_rb_realtime_stream_toggled(bool checked); // “实时数据流”单选按钮

    // 故障诊断算法训练
    void on_btn_start3_clicked();   // 开始训练
    void on_btn_stop3_clicked();    // 停止训练
    void on_btn_result3_2_clicked(); // 开始预测（单条推理数据库数据）
    void onTrainProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTrainProcessReadyRead();
    void onInferProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onInferProcessReadyRead();

private:
    Ui::MainWindow* ui;
    QSqlDatabase m_db;
    QString file_path; // 文件路径

    QThread* m_workerThread;  // 子线程对象
    ThreadWorker* m_worker;   // 子线程工作对象

    // ========== 主表格（tableView）状态 ==========
    QStandardItemModel* model;
    QStandardItemModel* m_diagModel;
    int m_pageSize;
    int m_currentPage;
    int m_totalRows;
    int m_totalPages;
    QStringList m_tableHeaders;
    QString m_searchKeyword;

    // ========== 数据库表格（tableView_database）状态 ==========
    QStandardItemModel* m_filterModel;       // 独立筛选模型
    QSet<int> m_visibleColumns;              // 列筛选状态
    int m_pageSize_2;                   // 分页大小（独立）
    int m_currentPage_2;                 // 当前页（独立）
    int m_totalRows_2;                   // 总行数（独立）
    int m_totalPages_2;                  // 总页数（独立）
    QString m_searchKeyword_2;               // 搜索关键词（独立）

    // 图表
    QChart* m_chart; // 图表对象
    QList<QLineSeries*> m_seriesList; // 线系列表
    QList<QCheckBox*> m_varCheckBoxes; // 存储动态生成的勾选框
    QMap<QString, QLineSeries*> m_varSeriesMap; // 表头名-曲线映射（用于勾选控制）、
    QValueAxis* m_xAxis; // X轴
    QValueAxis* m_yAxis; // Y轴
    int min_x = 0; // X轴范围最小值
    int max_x = 0; // X轴范围最大值
    QThread* m_chartWorkerThread; // 数据处理线程
    ChartDataWorker* m_chartWorker; // 数据处理工作类
    float m_workerGlobalYMin; // 缓存子线程的全局Y最小值
    float m_workerGlobalYMax; // 缓存子线程的全局Y最大值
    int m_loadedFieldCount; // 已加载字段数
    int m_totalFieldCount; // 总字段数
    QStringList m_checkedFields; // 实时维护勾选的字段列表
    QList<QColor> m_fieldColors; // 字段颜色列表

    //动态刷新
    QTimer* m_realtimeTimer;  // 实时流定时器
    int m_currentStreamIndex; // 当前滑动窗口的起始索引
    int m_streamWindowSize;   // 窗口大小（固定20条）

    // 故障诊断算法训练/推理
    QProcess* m_trainProcess;      // Python训练进程
    QProcess* m_inferProcess;      // Python推理进程
    bool m_trainingInProgress;     // 训练进行中标志
    bool m_inferInProgress;        // 推理进行中标志
    int m_inferCurrentRow;         // 当前推理行号
    QStringList m_inferFieldNames; // 推理用的特征字段名
    //视频流区域
    //QMediaPlayer* m_player;
    //QAudioOutput* m_audioOutput;
    //QVideoWidget* m_videoWidget;
    // 初始化数据库
    bool initDatabase();
    // 插入数据库
    bool insertDataToDb(const QStringList& headers, const QList<QStringList>& data);

    //主表格
    void initTableWidget(); // 初始化表格
    void initTableWidget_diagnosis();
    void initPageControl(); // 初始化分页控件
    void calculateTotalPages(); // 计算总页数
    int getTotalRows(); // 获取总行数
    void loadPageData(int pageNum); // 加载指定页数据
    void updatePageInfo(); // 更新分页信息标签
    void filterTable(const QString& keyword); // 表格过滤

    //数据库表格
    void initTableWidget_2(); // 初始化表格
    void copyModelToFilterModel();    // 拷贝主模型到筛选模型
    void applyColumnFilter();         // 应用列筛选（隐藏/显示列）
    void setColumnVisible(int colIndex, bool visible); // 单独设置某列是否显示
    int getTotalRows_2();                    // 统计筛选模型总行数（带搜索）
    void calculateTotalPages_2();            // 计算筛选模型总页数
    void loadPageData_2(int pageNum);        // 加载筛选模型分页数据
    void updatePageInfo_2();                 // 更新筛选模型分页信息
    void filterTable_2(const QString& keyword); // 筛选模型搜索

    // 图表
    void initChart(); // 初始化图表
    void initRangeSlider(); // 初始化范围滑动条
    void initChartWorker(); // 初始化数据处理线程
    void createVarCheckBoxes();// 创建复选框

    //测试函数
    void checkAllDatabaseTables();// 检查数据库中的所有表
    void initVideoWidget();
    void setupVideoPlayer(QVBoxLayout* layout, const QString& videoPath); // 通用视频加载器函数
};

#endif // MAINWINDOW_H
