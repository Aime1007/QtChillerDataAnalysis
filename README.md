# QtChillerDataAnalysis

基于多源数据融合的中央空调冷水机组故障诊断系统

## 项目简介

本项目是一个基于 Qt6/C++ 的桌面应用程序，用于中央空调冷水机组的运行数据分析与故障诊断。系统支持从 Excel 文件导入运行数据，提供交互式数据可视化、分页表格浏览、实时数据流模拟，并集成 CNN-BiLSTM 深度学习模型进行故障诊断。

## 功能特性

- **Excel 数据导入** — 基于 QXlsx 库实现 `.xlsx` 文件的高效解析与导入
- **数据表格展示** — 支持分页浏览、关键词搜索、列筛选的交互式主数据表格
- **数据库存储** — 使用 SQLite 持久化存储导入的数据，支持历史数据回溯
- **交互式曲线图** — 基于 Qt Charts，支持动态勾选变量、范围滑动条缩放、图表保存
- **实时数据流** — 模拟实时数据流刷新，以固定窗口（20 条）动态更新图表
- **故障诊断模型** — 集成 CNN-BiLSTM 深度学习模型，通过 QProcess 调用 Python 脚本完成训练与推理

## 技术栈

| 技术 | 说明 |
|------|------|
| Qt 6 | GUI 框架（Core / Gui / Widgets / Sql / Charts） |
| C++17 | 核心业务逻辑 |
| QXlsx | 第三方 Excel 读写库 (3rdparty) |
| SQLite | 本地数据库存储 |
| TensorFlow/Keras | CNN-BiLSTM 故障诊断模型 |
| Python 3 | 模型训练与推理脚本 |
| CMake | 构建系统 |

## 项目结构

```
QtDataAnalysis/
├── main.cpp                # 应用程序入口
├── mainwindow.cpp/h/ui     # 主窗口（数据表格、图表、实时流、故障诊断）
├── ChartDataWorker.cpp/h   # 图表数据加载工作线程
├── threadworker.cpp/h      # 通用异步任务工作线程
├── RangeSlider.cpp/h       # 自定义范围滑动条控件
├── customheaderview.cpp/h  # 自定义表头视图
├── cnn_bilstm.py           # CNN-BiLSTM 训练/推理脚本
├── CMakeLists.txt          # CMake 构建配置
├── res.qrc                 # 资源文件（图标、图片）
├── 3rdparty/QXlsx/         # QXlsx Excel 读写库源码
├── images/                 # UI 图标资源
└── build/                  # 构建输出目录（已忽略）
```

## 构建说明

### 环境要求

- Qt 6.x（Core、Gui、Widgets、Sql、Charts）
- CMake 3.20+
- 支持 C++17 的编译器（MSVC 2019+ / GCC 9+ / Clang 10+）
- Python 3.8+（用于故障诊断功能）
- TensorFlow、pandas、scikit-learn、openpyxl（Python 依赖）

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/Aime1007/QtChillerDataAnalysis.git
cd QtChillerDataAnalysis

# 构建
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## 使用说明

### 数据导入
1. 点击「上传」按钮选择 `.xlsx` 数据文件
2. 系统自动解析文件并在主表格中分页展示
3. 导入的数据自动存入 SQLite 数据库

### 数据曲线
1. 在「数据曲线」页面选择需要展示的数据列
2. 使用底部范围滑动条调整 X 轴显示范围
3. 可通过勾选框动态显示/隐藏各变量曲线
4. 点击「保存图片」导出当前图表

### 实时数据流
1. 切换到「实时数据流」模式
2. 系统以固定窗口（20 条）在图表上滚动展示数据
3. 再次点击可停止滚动

### 故障诊断
1. 确保已导入训练数据（Excel）
2. 点击「开始训练」调用 CNN-BiLSTM 模型进行训练
3. 训练完成后点击「结果预测」对数据库中的数据进行推理
4. 训练和推理过程的日志会实时显示在界面上

### CLI 调用模型

```bash
# 训练模式
python cnn_bilstm.py --mode train --data <xlsx_path> [--output_dir <dir>]

# 推理模式
python cnn_bilstm.py --mode infer --features <comma_separated> --model_dir <dir>
```

## 许可证

本项目仅用于学习与研究目的。
