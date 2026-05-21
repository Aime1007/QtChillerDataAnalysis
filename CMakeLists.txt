cmake_minimum_required(VERSION 3.20)
project(QtDataAnalysis VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 配置
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Charts Multimedia MultimediaWidgets)

# 自动处理Qt的UI/MOC/RCC文件
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # 添加头文件搜索路径

# 添加QXlsx源码目录（指向你解压的3rdparty/QXlsx）
add_subdirectory(3rdparty/QXlsx)

set(SOURCES
    main.cpp
    mainwindow.cpp
    customheaderview.cpp
    threadworker.cpp
    RangeSlider.cpp
    ChartDataWorker.cpp
)
set(HEADERS
    mainwindow.h
    customheaderview.h
    threadworker.h
    RangeSlider.h
    RangeSlider_p.h
    ChartDataWorker.h
)
set(UIS
    mainwindow.ui
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UIS}
    icon.rc
    res.qrc
)

target_link_libraries(${PROJECT_NAME} PRIVATE

    # Qt库（Qt6/Qt5对应修改）
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Charts
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    # 链接本地QXlsx库
    QXlsx::QXlsx
)
