#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QMetaObject>
#include <QApplication>
#include <QGraphicsLayout>
#include <QSqlRecord>
#include <QFileDialog>
#include <QCheckBox>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QFile>
#include "customheaderview.h"
#include "ChartDataWorker.h"
#include <QTimer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_workerThread(new QThread(this))
    , m_worker(new ThreadWorker())
    , m_filterModel(new QStandardItemModel(this))
    , m_chartWorkerThread(nullptr)
    , m_chartWorker(nullptr)
    , m_chart(nullptr)
    , m_xAxis(nullptr)
    , m_yAxis(nullptr)
    , min_x(0)
    , max_x(0)
    , m_currentPage(1)
    , m_totalPages(1)
    , m_totalRows(0)
    , m_pageSize(10)
    , m_currentPage_2(1)
    , m_totalPages_2(1)
    , m_totalRows_2(0)
    , m_pageSize_2(10)
    , m_trainProcess(nullptr)
    , m_inferProcess(nullptr)
    , m_trainingInProgress(false)
    , m_inferInProgress(false)
    , m_inferCurrentRow(0)
{
    ui->setupUi(this);

    // 确保窗口及内部布局均可自由缩放
    this->setMinimumSize(800, 600);
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    ui->centralwidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->centralwidget->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    ui->stackedWidget_main->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui->chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 解除所有页面的布局固定约束
    for (int i = 0; i < ui->stackedWidget_main->count(); ++i) {
        QWidget* page = ui->stackedWidget_main->widget(i);
        if (page && page->layout())
            page->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    }

    m_realtimeTimer = new QTimer(this);
    connect(m_realtimeTimer, &QTimer::timeout, this, &MainWindow::onRealtimeTimeout);
    m_currentStreamIndex = 4781; // 设定的初始起点
    m_streamWindowSize = 20;     // 每次展示 20 条数据

    initTableWidget();
    initTableWidget_2();
    initTableWidget_diagnosis();
    initChart();
    initChartWorker();
    initVideoWidget();

    if (!initDatabase()) {
        qDebug() << "数据库初始化失败：" << m_db.lastError().text();
        QMessageBox::critical(this, "错误", "数据库初始化失败！");
    }
    qDebug() << "数据库初始化成功";

    m_worker->moveToThread(m_workerThread);
    connect(this, &MainWindow::destroyed, m_workerThread, &QThread::quit);
    connect(this, &MainWindow::destroyed, m_worker, &ThreadWorker::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);
    connect(m_worker, &ThreadWorker::finished, this, &MainWindow::onWorkerFinished);
    connect(m_worker, &ThreadWorker::progressUpdated, this, &MainWindow::onProgressUpdated);

    m_workerThread->start();

    if (ui->progressBar) {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(0);
    }
}

MainWindow::~MainWindow()
{
    if (m_chartWorkerThread) {
        if (m_chartWorkerThread->isRunning()) {
            m_chartWorkerThread->quit();
            m_chartWorkerThread->wait();
        }
    }
    ChartDataWorker::clearConnectionPool();

    if (m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }

    if (m_db.isOpen()) {
        m_db.close();
    }

    for (QLineSeries* series : m_seriesList) {
        if (m_chart)
            m_chart->removeSeries(series);
        delete series;
    }
    m_seriesList.clear();
    m_varSeriesMap.clear();

    delete m_xAxis;
    delete m_yAxis;
    delete m_chart;
    delete ui;
}

bool MainWindow::initDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("./data.db");
    if (!m_db.open()) {
        qDebug() << "数据库打开失败：" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query;
    query.exec("DROP TABLE IF EXISTS excel_data");
    return true;
}

void MainWindow::on_btn_browse_clicked()
{
    QString file_path = QFileDialog::getOpenFileName(this, "选择Excel文件", "", "Excel文件 (*.xlsx)");
    if (file_path.isEmpty()) return;

    ui->file_path->setText(file_path);
    ui->btn_browse->setEnabled(false);
    if (ui->progressBar) {
        ui->progressBar->setValue(0);
    }
}

void MainWindow::loadDataToTable()
{
    if (!ui->tableView || !m_db.isOpen()) return;

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableView->model());
    if (!model) {
        model = new QStandardItemModel(this);
        ui->tableView->setModel(model);
    }
    model->clear();

    QSqlQuery query;
    if (!query.exec("PRAGMA table_info(excel_data)")) {
        qDebug() << "获取表头失败：" << query.lastError().text();
        return;
    }

    m_tableHeaders.clear();
    while (query.next()) {
        m_tableHeaders.append(query.value(1).toString());
    }
    model->setHorizontalHeaderLabels(m_tableHeaders);

    calculateTotalPages();
    loadPageData(1);
    initRangeSlider();
}

void MainWindow::onWorkerFinished(bool success, const QString& msg, const QStringList& headers, const QList<QStringList>& data)
{
    ui->btn_browse->setEnabled(true);
    if (!success) {
        QMessageBox::warning(this, "导入失败", msg);
        return;
    }

    if (insertDataToDb(headers, data)) {
        QMessageBox::information(this, "导入成功", QString("%1，已插入数据库！").arg(msg));
        loadDataToTable();
        createVarCheckBoxes();
        on_btn_show_chart_clicked();
    }
    else {
        QMessageBox::warning(this, "插入失败", "Excel解析成功，但插入数据库失败！");
    }

    if (ui->progressBar) {
        ui->progressBar->setValue(100);
    }
}

void MainWindow::onProgressUpdated(int progress)
{
    if (ui->progressBar) {
        ui->progressBar->setValue(progress);
    }
    ui->statusbar->showMessage(QString("正在加载图表数据... %1%").arg(progress), 0);
}

void MainWindow::onErrorOccurred(QString errorMsg)
{
    QMessageBox::warning(this, "错误", errorMsg);
}

bool MainWindow::insertDataToDb(const QStringList& headers, const QList<QStringList>& data)
{
    if (headers.isEmpty() || data.isEmpty()) {
        qDebug() << "无表头或数据可插入";
        return false;
    }

    QSqlQuery query;
    query.exec("DROP TABLE IF EXISTS excel_data");

    QString createSql = "CREATE TABLE IF NOT EXISTS excel_data (";
    for (int i = 0; i < headers.size(); ++i) {
        createSql += "`" + headers[i] + "` TEXT";
        if (i != headers.size() - 1) {
            createSql += ", ";
        }
    }
    createSql += ")";
    if (!query.exec(createSql)) {
        qDebug() << "创建表失败：" << query.lastError().text();
        return false;
    }

    query.prepare("INSERT INTO excel_data VALUES (" + QString("?, ").repeated(headers.size()).chopped(2) + ")");
    m_db.transaction();
    bool insertOk = true;

    for (int i = 0; i < data.size(); ++i) {
        const QStringList& row = data[i];
        for (int j = 0; j < row.size() && j < headers.size(); ++j) {
            query.bindValue(j, row[j]);
        }
        if (!query.exec()) {
            insertOk = false;
            qDebug() << "插入行失败：" << query.lastError().text() << "行号：" << i + 1;
            break;
        }
    }

    if (insertOk) {
        m_db.commit();
        qDebug() << "成功插入" << data.size() << "行数据";
    }
    else {
        m_db.rollback();
    }

    return insertOk;
}

void MainWindow::initTableWidget()
{
    if (!ui->tableView) return;

    QStandardItemModel* model = new QStandardItemModel(this);
    ui->tableView->setModel(model);

    CustomHeaderView* header = new CustomHeaderView(Qt::Horizontal, ui->tableView);
    ui->tableView->setHorizontalHeader(header);

    initPageControl();
}

void MainWindow::initTableWidget_diagnosis()
{
    if (!ui->tableView_diagnosis) return;

    // 1. 创建并绑定数据模型
    m_diagModel = new QStandardItemModel(this);
    ui->tableView_diagnosis->setModel(m_diagModel);

    // 2. 复用你之前的自定义表头
    CustomHeaderView* header = new CustomHeaderView(Qt::Horizontal, ui->tableView_diagnosis);
    ui->tableView_diagnosis->setHorizontalHeader(header);

    // 3. 设置和之前页面一样的样式和行为
    ui->tableView_diagnosis->setAlternatingRowColors(true); // 交替行颜色
    ui->tableView_diagnosis->setSelectionBehavior(QAbstractItemView::SelectRows); // 选中整行
    ui->tableView_diagnosis->setEditTriggers(QAbstractItemView::NoEditTriggers); // 禁止直接编辑
    ui->tableView_diagnosis->horizontalHeader()->setStretchLastSection(true); // 表头自适应拉伸
}

void MainWindow::initPageControl()
{
    ui->cbx_page_size->addItems({ "10", "20", "50" });
    ui->cbx_page_size->setCurrentText("10");
    m_pageSize = 10;
    ui->btn_prev_page->setEnabled(false);
    ui->btn_next_page->setEnabled(false);
}

void MainWindow::calculateTotalPages()
{
    if (m_pageSize <= 0) m_pageSize = 10;
    m_totalRows = getTotalRows();
    m_totalPages = (m_totalRows + m_pageSize - 1) / m_pageSize;

    if (m_currentPage > m_totalPages) {
        m_currentPage = m_totalPages > 0 ? m_totalPages : 1;
    }
}

int MainWindow::getTotalRows()
{
    if (!m_db.isOpen()) return 0;

    QSqlQuery query;
    QString countSql = "SELECT COUNT(*) FROM excel_data";

    if (!m_searchKeyword.isEmpty()) {
        countSql += " WHERE ";
        for (int i = 0; i < m_tableHeaders.size(); ++i) {
            if (i > 0) countSql += " OR ";
            countSql += QString("`%1` LIKE '%%2%'").arg(m_tableHeaders[i]).arg(m_searchKeyword);
        }
    }

    if (query.exec(countSql)) {
        if (query.next()) {
            return query.value(0).toInt();
        }
    }
    else {
        qDebug() << "统计行数失败：" << query.lastError().text();
    }
    return 0;
}

void MainWindow::loadPageData(int pageNum)
{
    if (!ui->tableView || !m_db.isOpen() || m_tableHeaders.isEmpty()) return;
    if (pageNum < 1) pageNum = 1;
    if (pageNum > m_totalPages) pageNum = m_totalPages;

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableView->model());
    if (!model) {
        model = new QStandardItemModel(this);
        ui->tableView->setModel(model);
    }
    model->removeRows(0, model->rowCount());

    int offset = (pageNum - 1) * m_pageSize;
    int limit = m_pageSize;
    QString selectSql = "SELECT * FROM excel_data";

    if (!m_searchKeyword.isEmpty()) {
        selectSql += " WHERE ";
        for (int i = 0; i < m_tableHeaders.size(); ++i) {
            if (i > 0) selectSql += " OR ";
            selectSql += QString("`%1` LIKE '%%2%'").arg(m_tableHeaders[i]).arg(m_searchKeyword);
        }
    }

    selectSql += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);

    QSqlQuery query;
    if (!query.exec(selectSql)) {
        qDebug() << "分页查询失败：" << query.lastError().text();
        return;
    }

    int row = 0;
    while (query.next()) {
        for (int col = 0; col < m_tableHeaders.size(); ++col) {
            QString cellValue = query.value(col).toString();
            QStandardItem* item = new QStandardItem(cellValue);
            model->setItem(row, col, item);
        }
        row++;
    }

    m_currentPage = pageNum;
    updatePageInfo();
    ui->btn_prev_page->setEnabled(m_currentPage > 1);
    ui->btn_next_page->setEnabled(m_currentPage < m_totalPages);

    copyModelToFilterModel();
}

void MainWindow::updatePageInfo()
{
    QString info = QString("第%1页/共%2页 总计%3行").arg(m_currentPage).arg(m_totalPages).arg(m_totalRows);
    ui->lab_page_info->setText(info);
}

void MainWindow::filterTable(const QString& keyword)
{
    m_searchKeyword = keyword.trimmed();
    calculateTotalPages();
    loadPageData(1);
}

void MainWindow::initTableWidget_2()
{
    ui->tableView_database->setModel(m_filterModel);
    ui->tableView_database->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->cbx_page_size_2->addItems({ "10", "20", "50" });
    ui->cbx_page_size_2->setCurrentText("10");
    m_pageSize_2 = 10;
    ui->btn_prev_page_2->setEnabled(false);
    ui->btn_next_page_2->setEnabled(false);

    CustomHeaderView* header_2 = new CustomHeaderView(Qt::Horizontal, ui->tableView_database);
    ui->tableView_database->setHorizontalHeader(header_2);
}
/*
void MainWindow::copyModelToFilterModel()
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableView->model());
    if (!model || !m_filterModel) return;

    QSet<int> tempVisible = m_visibleColumns;
    m_filterModel->clear();
    m_filterModel->setHorizontalHeaderLabels(m_tableHeaders);

    for (int row = 0; row < model->rowCount(); ++row) {
        QList<QStandardItem*> items;
        for (int col = 0; col < model->columnCount(); ++col) {
            auto* src = model->item(row, col);
            items.append(src ? new QStandardItem(src->text()) : new QStandardItem(""));
        }
        m_filterModel->appendRow(items);
    }

    m_diagModel->clear();
    m_diagModel->setHorizontalHeaderLabels(m_tableHeaders);

    for (int row = 0; row < model->rowCount(); ++row) {
        QList<QStandardItem*> items;
        for (int col = 0; col < model->columnCount(); ++col) {
            auto* src = model->item(row, col);
            items.append(src ? new QStandardItem(src->text()) : new QStandardItem(""));
        }
        m_diagModel->appendRow(items);
    }
    m_visibleColumns = tempVisible;
    if (m_visibleColumns.isEmpty()) {
        for (int c = 0; c < m_filterModel->columnCount(); ++c)
            m_visibleColumns.insert(c);
    }
    applyColumnFilter();

    calculateTotalPages_2();
    loadPageData_2(m_currentPage_2);
}
*/
void MainWindow::copyModelToFilterModel()
{
    if (!m_filterModel || !m_diagModel) return;

    // --- 第一步：同步【数据库数据】页面 ---
    // 保存列显示状态
    QSet<int> tempVisible = m_visibleColumns;

    // 设置筛选模型的表头
    m_filterModel->setHorizontalHeaderLabels(m_tableHeaders);

    // 触发数据库界面的分页查询（loadPageData_2 会自动清空原数据并执行 LIMIT/OFFSET 数据库查询）
    calculateTotalPages_2();
    loadPageData_2(m_currentPage_2);

    // 恢复并应用列过滤规则
    m_visibleColumns = tempVisible;
    if (m_visibleColumns.isEmpty()) {
        for (int c = 0; c < m_filterModel->columnCount(); ++c)
            m_visibleColumns.insert(c);
    }
    applyColumnFilter();


    // --- 第二步：同步【故障诊断结果】页面 ---
    // 抛弃以前从 ui->tableView 复制那 10 条数据的笨办法
    // 诊断页面没有分页控件，因此直接执行 SELECT * 查询所有行
    if (m_db.isOpen() && !m_tableHeaders.isEmpty()) {
        m_diagModel->clear();
        m_diagModel->setHorizontalHeaderLabels(m_tableHeaders);

        QSqlQuery query("SELECT * FROM excel_data");
        while (query.next()) {
            QList<QStandardItem*> itemsForDiag;
            for (int col = 0; col < m_tableHeaders.size(); ++col) {
                QString text = query.value(col).toString();
                itemsForDiag.append(new QStandardItem(text));
            }
            m_diagModel->appendRow(itemsForDiag);
        }
    }
}
void MainWindow::applyColumnFilter()
{
    if (!ui->tableView_database || !m_filterModel) return;
    QHeaderView* header = ui->tableView_database->horizontalHeader();
    for (int c = 0; c < m_filterModel->columnCount(); ++c) {
        header->setSectionHidden(c, !m_visibleColumns.contains(c));
    }
}

void MainWindow::setColumnVisible(int colIndex, bool visible)
{
    if (colIndex < 0 || colIndex >= m_filterModel->columnCount()) return;
    if (visible) m_visibleColumns.insert(colIndex);
    else m_visibleColumns.remove(colIndex);
    applyColumnFilter();
}

void MainWindow::on_cbx_page_size_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    m_pageSize = ui->cbx_page_size->currentText().toInt();
    calculateTotalPages();
    loadPageData(1);
}

void MainWindow::on_btn_prev_page_clicked()
{
    if (m_currentPage > 1)
        loadPageData(m_currentPage - 1);
}

void MainWindow::on_btn_next_page_clicked()
{
    if (m_currentPage < m_totalPages)
        loadPageData(m_currentPage + 1);
}

void MainWindow::on_btn_jump_page_clicked()
{
    bool ok;
    int page = ui->le_page_num->text().toInt(&ok);
    if (ok && page >= 1 && page <= m_totalPages) {
        loadPageData(page);
        ui->le_page_num->clear();
    }
    else {
        QMessageBox::warning(this, "提示", "无效页码！");
    }
}

void MainWindow::on_btn_upload_clicked()
{
    QString path = ui->file_path->text();
    if (path.isEmpty()) return;
    QMetaObject::invokeMethod(m_worker, "doImportXlsx", Qt::QueuedConnection, Q_ARG(QString, path));
}

void MainWindow::on_le_search_textChanged(const QString& arg1)
{
    filterTable(arg1);
}

int MainWindow::getTotalRows_2()
{
    if (!m_db.isOpen()) return 0;
    QSqlQuery query;
    QString sql = "SELECT COUNT(*) FROM excel_data";
    if (!m_searchKeyword_2.isEmpty()) {
        sql += " WHERE ";
        for (int i = 0; i < m_tableHeaders.size(); ++i) {
            if (i > 0) sql += " OR ";
            sql += QString("`%1` LIKE '%%2%'").arg(m_tableHeaders[i]).arg(m_searchKeyword_2);
        }
    }
    if (query.exec(sql) && query.next()) return query.value(0).toInt();
    return 0;
}

void MainWindow::calculateTotalPages_2()
{
    if (m_pageSize_2 <= 0) m_pageSize_2 = 10;
    m_totalRows_2 = getTotalRows_2();
    m_totalPages_2 = (m_totalRows_2 + m_pageSize_2 - 1) / m_pageSize_2;
    if (m_currentPage_2 > m_totalPages_2)
        m_currentPage_2 = m_totalPages_2 > 0 ? m_totalPages_2 : 1;
}

void MainWindow::loadPageData_2(int pageNum)
{
    if (!ui->tableView_database || !m_db.isOpen() || m_tableHeaders.isEmpty()) return;
    if (pageNum < 1) pageNum = 1;
    if (pageNum > m_totalPages_2) pageNum = m_totalPages_2;

    m_filterModel->removeRows(0, m_filterModel->rowCount());
    m_filterModel->setHorizontalHeaderLabels(m_tableHeaders);
    int offset = (pageNum - 1) * m_pageSize_2;
    int limit = m_pageSize_2;
    QString sql = "SELECT * FROM excel_data";

    if (!m_searchKeyword_2.isEmpty()) {
        sql += " WHERE ";
        for (int i = 0; i < m_tableHeaders.size(); ++i) {
            if (i > 0) sql += " OR ";
            sql += QString("`%1` LIKE '%%2%'").arg(m_tableHeaders[i]).arg(m_searchKeyword_2);
        }
    }
    sql += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);

    QSqlQuery query;
    if (query.exec(sql)) {
        int row = 0;
        while (query.next()) {
            for (int col = 0; col < m_tableHeaders.size(); ++col) {
                m_filterModel->setItem(row, col, new QStandardItem(query.value(col).toString()));
            }
            row++;
        }
    }

    applyColumnFilter();
    m_currentPage_2 = pageNum;
    updatePageInfo_2();
    ui->btn_prev_page_2->setEnabled(m_currentPage_2 > 1);
    ui->btn_next_page_2->setEnabled(m_currentPage_2 < m_totalPages_2);
}

void MainWindow::updatePageInfo_2()
{
    QString info = QString("第%1页/共%2页 总计%3行").arg(m_currentPage_2).arg(m_totalPages_2).arg(m_totalRows_2);
    ui->lab_page_info_2->setText(info);
}

void MainWindow::filterTable_2(const QString& keyword)
{
    m_searchKeyword_2 = keyword.trimmed();
    calculateTotalPages_2();
    loadPageData_2(1);
}

void MainWindow::on_le_search_2_textChanged(const QString& arg1)
{
    filterTable_2(arg1);
}

void MainWindow::on_cbx_page_size_2_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    m_pageSize_2 = ui->cbx_page_size_2->currentText().toInt();
    calculateTotalPages_2();
    loadPageData_2(1);
}

void MainWindow::on_btn_prev_page_2_clicked()
{
    if (m_currentPage_2 > 1)
        loadPageData_2(m_currentPage_2 - 1);
}

void MainWindow::on_btn_next_page_2_clicked()
{
    if (m_currentPage_2 < m_totalPages_2)
        loadPageData_2(m_currentPage_2 + 1);
}

void MainWindow::on_btn_jump_page_2_clicked()
{
    bool ok;
    int page = ui->le_page_num_2->text().toInt(&ok);
    if (ok && page >= 1 && page <= m_totalPages_2) {
        loadPageData_2(page);
        ui->le_page_num_2->clear();
    }
    else {
        QMessageBox::warning(this, "提示", "无效页码！");
    }
}

void MainWindow::initChart()
{
    m_chart = new QChart();
    m_xAxis = new QValueAxis();
    m_yAxis = new QValueAxis();

    m_xAxis->setTitleText("数据点");
    m_xAxis->setLabelFormat("%d");
    m_yAxis->setTitleText("数值");

    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);

    ui->chartView->setChart(m_chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::initRangeSlider()
{
    ui->slider_data_range->setMinimum(0);
    ui->slider_data_range->setMaximum(m_totalRows);
    ui->slider_data_range->setSpan(0, qMax(1, m_totalRows / 5));
    ui->slider_data_range->setTickInterval(qMax(1, m_totalRows / 10));
    ui->slider_data_range->setTickPosition(QSlider::TicksBelow);

    connect(ui->slider_data_range, &RangeSlider::spanChanged, this, &MainWindow::onSliderRangeChanged, Qt::QueuedConnection);
}

void MainWindow::initChartWorker()
{
    m_chartWorkerThread = new QThread(this);
    m_chartWorker = new ChartDataWorker("./data.db");
    m_chartWorker->moveToThread(m_chartWorkerThread);

    connect(m_chartWorker, &ChartDataWorker::fieldDataLoaded, this, &MainWindow::onFieldDataLoaded);
    connect(m_chartWorker, &ChartDataWorker::allDataLoaded, this, &MainWindow::onAllDataLoaded);
    connect(m_chartWorker, &ChartDataWorker::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_chartWorker, &ChartDataWorker::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(this, &MainWindow::startLoadSpecifiedFields,
        m_chartWorker, &ChartDataWorker::loadSpecifiedFields);

    connect(m_chartWorkerThread, &QThread::finished, m_chartWorker, &QObject::deleteLater);
    connect(m_chartWorkerThread, &QThread::finished, m_chartWorkerThread, &QObject::deleteLater);
}

void MainWindow::createVarCheckBoxes()
{
    if (!ui->gridLayout_vars || m_tableHeaders.isEmpty()) return;

    qDeleteAll(m_varCheckBoxes);
    m_varCheckBoxes.clear();
    m_fieldColors.clear();
    m_checkedFields.clear();

    while (auto* item = ui->gridLayout_vars->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    int cols = 8;
    int r = 0, c = 0;
    for (const QString& name : m_tableHeaders) {
        auto* cb = new QCheckBox(name, this);
        cb->setChecked(true);
        connect(cb, &QCheckBox::clicked, this, &MainWindow::onVarCheckBoxClicked);
        ui->gridLayout_vars->addWidget(cb, r, c);
        m_varCheckBoxes.append(cb);

        c++;
        if (c >= cols) { c = 0; r++; }
    }

    // 生成一次固定颜色
    for (int i = 0; i < m_tableHeaders.size(); ++i) {
        int rColor = QRandomGenerator::global()->bounded(70, 240);
        int gColor = QRandomGenerator::global()->bounded(70, 240);
        int bColor = QRandomGenerator::global()->bounded(70, 240);
        m_fieldColors.append(QColor(rColor, gColor, bColor));
    }

    m_checkedFields = m_tableHeaders;
}

void MainWindow::checkAllDatabaseTables()
{
    if (!m_db.isOpen()) return;
    QSqlQuery q("SELECT name FROM sqlite_master WHERE type='table'");
    while (q.next()) qDebug() << "表：" << q.value(0).toString();
}

void MainWindow::refreshChart()
{
    if (!m_chart || m_totalRows <= 0) {
        ui->statusbar->showMessage("无数据可刷新", 3000);
        return;
    }
    if (m_checkedFields.isEmpty()) {
        ui->statusbar->showMessage("请至少勾选一个字段！", 3000);
        return;
    }

    ui->chartView->setUpdatesEnabled(false);

    // 安全清空所有曲线
    for (QAbstractSeries* s : m_chart->series()) {
        m_chart->removeSeries(s);
    }
    qDeleteAll(m_seriesList);
    m_seriesList.clear();
    m_varSeriesMap.clear();

    int loadMin = min_x;
    int loadMax = max_x;

    if (loadMin < 0) loadMin = 0;
    if (loadMax >= m_totalRows) loadMax = m_totalRows - 1;
    if (loadMin >= loadMax) loadMax = loadMin + 50;

    m_xAxis->setRange(loadMin, loadMax);
    emit startLoadSpecifiedFields(loadMin, loadMax, m_checkedFields);

    ui->statusbar->showMessage("正在生成趋势图...");
}

void MainWindow::onVarCheckBoxClicked(bool checked)
{
    QCheckBox* cb = qobject_cast<QCheckBox*>(sender());
    if (!cb) return;

    QString name = cb->text();

    if (checked) {
        if (!m_checkedFields.contains(name))
            m_checkedFields.append(name);
    }
    else {
        m_checkedFields.removeOne(name);
    }

    QLineSeries* series = m_varSeriesMap.value(name);
    if (series) {
        series->setVisible(checked);
    }
}

void MainWindow::onSliderRangeChanged(int min, int max)
{
    min_x = min;
    max_x = max;
}

void MainWindow::on_btn_show_chart_clicked()
{
    refreshChart();
    if (!m_chartWorkerThread->isRunning())
        m_chartWorkerThread->start();
}

void MainWindow::onFieldDataLoaded(const FieldData& data)
{
    /*
    QLineSeries* series = new QLineSeries();
    series->setName(data.fieldName);
    series->replace(data.dataPoints);

    int index = m_tableHeaders.indexOf(data.fieldName);
    if (index >= 0 && index < m_fieldColors.size()) {
        QColor color = m_fieldColors[index];
        series->setColor(color);
        series->setPen(QPen(color, 2));
    }

    m_chart->addSeries(series);
    series->attachAxis(m_xAxis);
    series->attachAxis(m_yAxis);

    m_varSeriesMap[data.fieldName] = series;
    m_seriesList.append(series);
*/
    // 从 Map 中查找这个字段的曲线是否已经存在
    QLineSeries* series = m_varSeriesMap.value(data.fieldName, nullptr);

    if (!series) {
        // 如果不存在（第一次加载），才去 new
        series = new QLineSeries();
        series->setName(data.fieldName);

        int index = m_tableHeaders.indexOf(data.fieldName);
        if (index >= 0 && index < m_fieldColors.size()) {
            QColor color = m_fieldColors[index];
            series->setColor(color);
            series->setPen(QPen(color, 2));
        }

        m_chart->addSeries(series);
        series->attachAxis(m_xAxis);
        series->attachAxis(m_yAxis);

        m_varSeriesMap[data.fieldName] = series;
        m_seriesList.append(series);
    }

    // 复用已有曲线，直接全量替换数据点，图表会自动进行平滑刷新
    series->replace(data.dataPoints);
}

void MainWindow::onAllDataLoaded(float globalMin, float globalMax)
{
    float margin = (globalMax - globalMin) * 0.1f;
    m_yAxis->setRange(globalMin - margin, globalMax + margin);
    ui->chartView->setUpdatesEnabled(true);
    ui->statusbar->showMessage("图表加载完成！");
}
void MainWindow::on_btn_save_chart_clicked()
{
    // 没有图表就直接返回
    if (!m_chart || m_seriesList.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有可保存的图表！");
        return;
    }

    // 选择保存路径
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "保存图表",
        QDir::homePath() + "/chart.png",
        "PNG图片 (*.png);;JPG图片 (*.jpg);;BMP图片 (*.bmp)"
    );

    if (filePath.isEmpty())
        return;

    // 开始保存图片（高分辨率）
    QPixmap pixmap = ui->chartView->grab();
    pixmap.save(filePath);

    QMessageBox::information(this, "保存成功", "图表已保存至：\n" + filePath);
}


void MainWindow::on_btn_export_data_clicked()
{

}

void MainWindow::onRealtimeTimeout()
{
    // 如果没有勾选任何字段或没有数据，则不执行
    if (m_checkedFields.isEmpty() || m_totalRows <= 0) return;

    // 计算滑动窗口的起止点
    int loadMin = m_currentStreamIndex;
    int loadMax = m_currentStreamIndex + m_streamWindowSize - 1;

    // 越界保护：如果读到了数据库最后一条
    if (loadMax >= m_totalRows) {
        m_realtimeTimer->stop(); // 停止定时器
        ui->statusbar->showMessage("实时数据流已播放完毕！");
        return;
    }

    // 更新图表X轴范围，实现平滑滚动
    m_xAxis->setRange(loadMin, loadMax);

    // 通知子线程去查这 20 条数据（查完会自动触发 onFieldDataLoaded）
    emit startLoadSpecifiedFields(loadMin, loadMax, m_checkedFields);

    // 索引加1，为下一个 5 秒做准备
    m_currentStreamIndex++;
}

void MainWindow::on_rb_realtime_stream_toggled(bool checked)
{
    if (checked) {
        // 开启实时流模式
        m_currentStreamIndex = 4781; // 重置到起点

        // 立即执行第一帧画面
        onRealtimeTimeout();

        // 启动定时器，参数为毫秒
        m_realtimeTimer->start(2000);

        ui->statusbar->showMessage("实时数据流已开启...");

        // 可选：禁用掉范围滑动条，防止用户乱拖
        if (ui->slider_data_range) {
            ui->slider_data_range->setEnabled(false);
        }
    } else {
        // 关闭实时流模式
        if (m_realtimeTimer->isActive()) {
            m_realtimeTimer->stop();
        }
        ui->statusbar->showMessage("实时数据流已关闭。");
        if (ui->slider_data_range) {
            ui->slider_data_range->setEnabled(true);
        }
    }
}

void MainWindow::initVideoWidget()
{
    // 第 1 个视频
    setupVideoPlayer(ui->video_container, "D:/2QT/meter_1.mp4");

    // 第 2 个视频
    setupVideoPlayer(ui->video_container2, "D:/2QT/meter_2.mp4");

    // 第 3 个视频
    setupVideoPlayer(ui->video_container3, "D:/2QT/meter_3.mp4");
}

void MainWindow::setupVideoPlayer(QVBoxLayout *layout, const QString &videoPath)
{
    // 如果没有传入合法的布局，直接退出防崩溃
    if (!layout) return;

    // 每次调用这个函数，都会创建一套全新的播放器流水线
    // 传入 this，Qt 会在关闭窗口时自动帮我们销毁它们，不会内存泄漏
    QMediaPlayer* player = new QMediaPlayer(this);
    QAudioOutput* audioOutput = new QAudioOutput(this);
    QVideoWidget* videoWidget = new QVideoWidget(this);

    // 绑定输出
    player->setAudioOutput(audioOutput);
    player->setVideoOutput(videoWidget);

    // 直接把视频画面塞进传进来的布局里
    layout->addWidget(videoWidget);

    // 设置路径并无限循环
    player->setSource(QUrl::fromLocalFile(videoPath));
    player->setLoops(QMediaPlayer::Infinite);

    // 💡温馨提示：4个视频同时播放声音会打架，建议多路播放时设置为静音 (0.0)
    audioOutput->setVolume(0.0);

    // 启动！
    player->play();
}

// ============================================================
// 故障诊断算法训练 — CNN_BiLSTM
// ============================================================

// 查找 Python 可执行文件（优先 conda 环境）
static QString findPythonExe()
{
    QStringList candidates = {
        QDir::homePath() + "/Anaconda/envs/tf_fault_diagnosis/python.exe",
        "D:/Anaconda/envs/tf_fault_diagnosis/python.exe",
        "C:/Anaconda/envs/tf_fault_diagnosis/python.exe",
        "python",
        "python3"
    };
    for (const QString& py : candidates) {
        if (QFile::exists(py)) return py;
    }
    return "python";
}

// 查找 cnn_bilstm.py 脚本路径
static QString findScriptPath()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/../../cnn_bilstm.py",
        QCoreApplication::applicationDirPath() + "/../cnn_bilstm.py",
        QCoreApplication::applicationDirPath() + "/cnn_bilstm.py",
        "cnn_bilstm.py"
    };
    for (const QString& p : searchPaths) {
        if (QFile::exists(p)) return QFileInfo(p).absoluteFilePath();
    }
    return QString();
}

void MainWindow::on_btn_start3_clicked()
{
    // 检查算法选择是否为 CNN_BiLSTM
    QString algo = ui->cbx_layer_type_12->currentText();
    if (algo != "CNN_BiLSTM") {
        // 其他算法的训练逻辑（预留）
        ui->textEdit_model_structure->append(
            QString("[WARN] %1 算法尚未接入训练后端，请选择 CNN_BiLSTM").arg(algo));
        return;
    }

    // 让用户选择训练数据文件
    QString dataFile = QFileDialog::getOpenFileName(
        this, "选择训练数据Excel文件", QString(),
        "Excel Files (*.xlsx *.xls)");
    if (dataFile.isEmpty()) return;

    ui->textEdit_model_structure->clear();
    ui->textEdit_model_structure->append("[INFO] 算法: CNN_BiLSTM");
    ui->textEdit_model_structure->append("[INFO] 训练数据: " + dataFile);
    ui->textEdit_model_structure->append("[INFO] 开始训练...");

    ui->btn_start3->setEnabled(false);
    ui->btn_stop3->setEnabled(true);
    ui->progressBar_train_2->setValue(0);
    m_trainingInProgress = true;

    // 清理旧进程
    if (m_trainProcess) {
        m_trainProcess->kill();
        m_trainProcess->deleteLater();
    }

    m_trainProcess = new QProcess(this);
    connect(m_trainProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onTrainProcessFinished);
    connect(m_trainProcess, &QProcess::readyReadStandardOutput,
            this, &MainWindow::onTrainProcessReadyRead);
    connect(m_trainProcess, &QProcess::readyReadStandardError,
            this, &MainWindow::onTrainProcessReadyRead);

    QString pythonExe = findPythonExe();
    QString scriptPath = findScriptPath();
    if (scriptPath.isEmpty()) {
        ui->textEdit_model_structure->append("[ERROR] 找不到 cnn_bilstm.py 脚本文件！");
        return;
    }
    QString outputDir = QCoreApplication::applicationDirPath();

    ui->textEdit_model_structure->append("[INFO] Python: " + pythonExe);
    ui->textEdit_model_structure->append("[INFO] Script: " + scriptPath);

    QStringList args;
    args << scriptPath << "--mode" << "train"
         << "--data" << dataFile
         << "--output_dir" << outputDir;

    ui->textEdit_model_structure->append("[CMD] " + pythonExe + " " + args.join(" "));
    m_trainProcess->start(pythonExe, args);
}

void MainWindow::on_btn_stop3_clicked()
{
    if (m_trainProcess && m_trainProcess->state() == QProcess::Running) {
        m_trainProcess->kill();
        ui->textEdit_model_structure->append("[INFO] 训练已被用户停止");
        m_trainingInProgress = false;
    }
    ui->btn_start3->setEnabled(true);
    ui->btn_stop3->setEnabled(false);
}

void MainWindow::onTrainProcessReadyRead()
{
    if (!m_trainProcess) return;
    QString output = QString::fromLocal8Bit(m_trainProcess->readAllStandardOutput());
    QString errOutput = QString::fromLocal8Bit(m_trainProcess->readAllStandardError());
    if (!output.isEmpty()) {
        ui->textEdit_model_structure->append(output.trimmed());
    }
    if (!errOutput.isEmpty()) {
        ui->textEdit_model_structure->append("[STDERR] " + errOutput.trimmed());
    }

    // 更新进度条（根据epoch输出估算）
    static QRegularExpression epochRe("Epoch (\\d+)/500");
    QRegularExpressionMatch match = epochRe.match(output);
    if (match.hasMatch()) {
        int epoch = match.captured(1).toInt();
        int pct = qMin(epoch * 100 / 500, 100);
        ui->progressBar_train_2->setValue(pct);
    }
}

void MainWindow::onTrainProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_trainingInProgress = false;
    ui->btn_start3->setEnabled(true);
    ui->btn_stop3->setEnabled(false);

    // 读取所有剩余输出
    QString remainingOut = QString::fromLocal8Bit(m_trainProcess->readAllStandardOutput());
    QString remainingErr = QString::fromLocal8Bit(m_trainProcess->readAllStandardError());
    if (!remainingOut.trimmed().isEmpty()) {
        ui->textEdit_model_structure->append(remainingOut.trimmed());
    }
    if (!remainingErr.trimmed().isEmpty()) {
        ui->textEdit_model_structure->append("[STDERR] " + remainingErr.trimmed());
    }

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        ui->textEdit_model_structure->append(
            QString("[ERROR] 训练失败！exitCode=%1, exitStatus=%2")
                .arg(exitCode).arg(exitStatus == QProcess::CrashExit ? "Crash" : "Normal"));
        ui->progressBar_train_2->setValue(0);
        // 检查常见错误
        if (remainingErr.contains("No module named") || remainingErr.contains("ModuleNotFoundError")) {
            QMessageBox::critical(this, "依赖缺失",
                QString("Python 依赖包缺失！\n\n%1\n\n请运行: pip install tensorflow scikit-learn pandas openpyxl matplotlib seaborn")
                    .arg(remainingErr.trimmed()));
        }
        return;
    }

    ui->progressBar_train_2->setValue(100);
    ui->textEdit_model_structure->append("[INFO] 训练完成！模型已保存");

    // 更新 model 页面的模型选择下拉框
    QString modelFile = QCoreApplication::applicationDirPath() + "/cnn_bilstm_model.h5";
    if (QFile::exists(modelFile)) {
        // 添加模型到下拉框
        bool found = false;
        for (int i = 0; i < ui->cbx_layer_type_14->count(); ++i) {
            if (ui->cbx_layer_type_14->itemText(i) == "CNN_BiLSTM") {
                found = true;
                ui->cbx_layer_type_14->setCurrentIndex(i);
                break;
            }
        }
        if (!found) {
            ui->cbx_layer_type_14->addItem("CNN_BiLSTM");
            ui->cbx_layer_type_14->setCurrentText("CNN_BiLSTM");
        }
        QMessageBox::information(this, "训练完成",
                                 QString("CNN_BiLSTM 模型训练完成！\n模型已保存至: %1").arg(modelFile));
    }
}

// ============================================================
// 故障诊断推理 — 逐条推理数据库中的数据
// ============================================================
void MainWindow::on_btn_result3_2_clicked()
{
    // 检查是否选择了 CNN_BiLSTM 模型
    QString modelName = ui->cbx_layer_type_14->currentText();
    if (modelName != "CNN_BiLSTM") {
        ui->label_diagnosis->setText(
            QString("[WARN] 当前选择的模型 %1 尚未接入推理后端，请选择 CNN_BiLSTM").arg(modelName));
        return;
    }

    // 检查模型文件
    QString modelDir = QCoreApplication::applicationDirPath();
    QString modelFile = modelDir + "/cnn_bilstm_model.h5";
    if (!QFile::exists(modelFile)) {
        QMessageBox::warning(this, "提示", "未找到训练好的模型文件，请先训练 CNN_BiLSTM 模型！");
        return;
    }

    // 检查数据库是否有数据
    if (!m_db.isOpen() || m_tableHeaders.isEmpty()) {
        QMessageBox::warning(this, "提示", "数据库中没有数据，请先导入Excel数据！");
        return;
    }

    // 获取数据库总行数
    QSqlQuery countQuery("SELECT COUNT(*) FROM excel_data");
    int totalRows = 0;
    if (countQuery.exec() && countQuery.next()) {
        totalRows = countQuery.value(0).toInt();
    }
    if (totalRows == 0) {
        QMessageBox::warning(this, "提示", "数据库中没有数据！");
        return;
    }

    // 清空诊断表格，准备写入结果
    m_diagModel->removeRows(0, m_diagModel->rowCount());

    // 设置表头：原始字段 + 诊断结果
    QStringList diagHeaders;
    diagHeaders << m_tableHeaders << "诊断结果" << "置信度";
    m_diagModel->setHorizontalHeaderLabels(diagHeaders);

    // 让诊断结果区域可见
    ui->label_diagnosis->setText("正在推理中...");

    // 开始逐条推理
    m_inferInProgress = true;
    m_inferCurrentRow = 0;
    m_inferFieldNames = m_tableHeaders; // 所有特征字段

    ui->btn_result3_2->setEnabled(false);
    ui->statusbar->showMessage(QString("开始诊断推理，共 %1 条数据...").arg(totalRows));

    // 查询第一条数据并开始推理
    QSqlQuery query("SELECT * FROM excel_data LIMIT 1 OFFSET 0");
    if (query.exec() && query.next()) {
        QStringList featureValues;
        for (int col = 0; col < m_tableHeaders.size(); ++col) {
            featureValues << query.value(col).toString();
        }

        // 清理旧推理进程
        if (m_inferProcess) {
            m_inferProcess->kill();
            m_inferProcess->deleteLater();
        }

        m_inferProcess = new QProcess(this);
        connect(m_inferProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MainWindow::onInferProcessFinished);
        connect(m_inferProcess, &QProcess::readyReadStandardOutput,
                this, &MainWindow::onInferProcessReadyRead);

        QString pythonExe = findPythonExe();
        QString scriptPath = findScriptPath();
        if (scriptPath.isEmpty()) return;

        QStringList args;
        args << scriptPath << "--mode" << "infer"
             << "--features" << featureValues.join(",")
             << "--model_dir" << modelDir;

        m_inferProcess->start(pythonExe, args);
    }
}

void MainWindow::onInferProcessReadyRead()
{
    // 推理进程的输出在 onInferProcessFinished 中统一处理
}

void MainWindow::onInferProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_inferProcess || !m_inferInProgress) return;

    QString output = QString::fromLocal8Bit(m_inferProcess->readAllStandardOutput());
    QString errorOutput = QString::fromLocal8Bit(m_inferProcess->readAllStandardError());

    if (!errorOutput.isEmpty()) {
        qDebug() << "[INFER STDERR]" << errorOutput;
    }

    // 提取 [RESULT] 行中的JSON
    QString resultClass = "未知";
    QString confidence = "0";
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            if (line.startsWith("[RESULT]")) {
                QString jsonStr = line.mid(8).trimmed(); // 去掉 "[RESULT] " 前缀
                QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    resultClass = obj.value("class_name").toString("未知");
                    confidence = QString::number(obj.value("probability").toDouble(0) * 100, 'f', 1) + "%";
                }
                break;
            }
        }
    } else {
        resultClass = "推理失败";
        confidence = "N/A";
    }

    // 将结果写入诊断表格
    if (m_inferCurrentRow < m_diagModel->rowCount()) {
        // 更新已有行
        m_diagModel->setItem(m_inferCurrentRow, m_tableHeaders.size(),
                             new QStandardItem(resultClass));
        m_diagModel->setItem(m_inferCurrentRow, m_tableHeaders.size() + 1,
                             new QStandardItem(confidence));
    } else {
        // 需要从数据库查询当前行数据并添加到表格
        QSqlQuery rowQuery;
        rowQuery.exec(QString("SELECT * FROM excel_data LIMIT 1 OFFSET %1").arg(m_inferCurrentRow));
        if (rowQuery.next()) {
            QList<QStandardItem*> items;
            for (int col = 0; col < m_tableHeaders.size(); ++col) {
                items.append(new QStandardItem(rowQuery.value(col).toString()));
            }
            items.append(new QStandardItem(resultClass));
            items.append(new QStandardItem(confidence));
            m_diagModel->appendRow(items);
        }
    }

    // 更新诊断结果标签（显示最近一条）
    ui->label_diagnosis->setText(
        QString("第 %1 条 → 诊断结果: %2 (置信度: %3)")
            .arg(m_inferCurrentRow + 1).arg(resultClass).arg(confidence));

    // 推进到下一条
    m_inferCurrentRow++;

    // 检查是否还有下一条
    QSqlQuery countQuery("SELECT COUNT(*) FROM excel_data");
    int totalRows = 0;
    if (countQuery.exec() && countQuery.next()) {
        totalRows = countQuery.value(0).toInt();
    }

    if (m_inferCurrentRow >= totalRows) {
        // 全部推理完成
        m_inferInProgress = false;
        ui->btn_result3_2->setEnabled(true);
        ui->statusbar->showMessage(
            QString("诊断推理完成！共推理 %1 条数据").arg(totalRows));
        ui->label_diagnosis->setText(
            QString("诊断推理完成！共 %1 条数据，请查看下方表格。").arg(totalRows));
        return;
    }

    // 启动下一条推理
    QSqlQuery nextQuery;
    nextQuery.exec(QString("SELECT * FROM excel_data LIMIT 1 OFFSET %1").arg(m_inferCurrentRow));
    if (nextQuery.exec() && nextQuery.next()) {
        QStringList featureValues;
        for (int col = 0; col < m_tableHeaders.size(); ++col) {
            featureValues << nextQuery.value(col).toString();
        }

        if (m_inferProcess) {
            m_inferProcess->deleteLater();
        }

        m_inferProcess = new QProcess(this);
        connect(m_inferProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MainWindow::onInferProcessFinished);
        connect(m_inferProcess, &QProcess::readyReadStandardOutput,
                this, &MainWindow::onInferProcessReadyRead);

        QString pythonExe = findPythonExe();
        QString scriptPath = findScriptPath();
        if (scriptPath.isEmpty()) return;
        QString modelDir = QCoreApplication::applicationDirPath();

        QStringList args;
        args << scriptPath << "--mode" << "infer"
             << "--features" << featureValues.join(",")
             << "--model_dir" << modelDir;

        m_inferProcess->start(pythonExe, args);
    }

    // 更新进度
    ui->statusbar->showMessage(
        QString("诊断推理中... %1/%2").arg(m_inferCurrentRow + 1).arg(totalRows));
}
