#include "customheaderview.h"
#include <QTextLayout>

CustomHeaderView::CustomHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
    setSectionsClickable(true);   // 允许点击表头
}

CustomHeaderView::~CustomHeaderView()
{
}

void CustomHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const {
    if (rect.isEmpty()) return;

    painter->save();

    // 1. 绘制表头背景和边框（自定义样式）
    painter->fillRect(rect, palette().button().color());
    painter->drawRect(rect.adjusted(0, 0, -1, -1)); // 绘制边框，避免超出范围

    // 2. 获取原始表头文本 + 拼接排序箭头
    QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();

    // 判断是否为排序列，并拼接箭头符号
    if (sortIndicatorSection() == logicalIndex) {
        if (sortIndicatorOrder() == Qt::AscendingOrder) {
            text += " ↑"; // 升序加向上箭头
        }
        else {
            text += " ↓"; // 降序加向下箭头
        }
    }

    // 3. 绘制带箭头的换行文本（无需预留原生箭头区域）
    if (!text.isEmpty()) {
        QRect textRect = rect.adjusted(2, 2, -2, -2); // 仅保留少量内边距，无右侧箭头预留
        QTextOption option;
        option.setAlignment(Qt::AlignCenter); // 文本居中（也可改左对齐）
        option.setWrapMode(QTextOption::WordWrap); // 自动换行
        painter->setPen(QPen(Qt::black)); // 文本颜色（可自定义）
        painter->drawText(textRect, text, option);
    }

    painter->restore();
}

QSize CustomHeaderView::sectionSizeFromContents(int logicalIndex) const
{
    // 获取默认尺寸（基础宽度/高度）
    QSize defaultSize = QHeaderView::sectionSizeFromContents(logicalIndex);
    // 获取表头文本
    QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
    if (text.isEmpty()) {
        return defaultSize;
    }

    // 计算文本换行后的实际高度
    int textWidth = sectionSize(logicalIndex); // 当前表头的宽度（水平表头）
    // 如果是垂直表头，替换为高度
    if (orientation() == Qt::Vertical) {
        textWidth = sectionSize(logicalIndex);
    }

    // 用QTextLayout精确计算换行后的文本高度
    QFont font = this->font(); // 获取当前表头的字体
    QTextLayout layout(text, font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    int totalHeight = 0;
    while (line.isValid()) {
        line.setLineWidth(textWidth - 10); // 预留10px边距，避免文字贴边
        totalHeight += line.height();
        line = layout.createLine();
    }
    layout.endLayout();

    // 最终尺寸：宽度沿用默认/当前宽度，高度替换为计算后的高度
    QSize actualSize;
    if (orientation() == Qt::Horizontal) {
        // 水平表头：宽度不变，高度取计算值（最小不低于默认高度）
        actualSize = QSize(defaultSize.width(), qMax(totalHeight + 10, defaultSize.height()));
    }
    else {
        // 垂直表头：高度不变，宽度取计算值
        actualSize = QSize(qMax(totalHeight + 10, defaultSize.width()), defaultSize.height());
    }

    return actualSize;
}

