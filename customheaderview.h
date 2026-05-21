#include <QHeaderView>
#include <QPainter>

class CustomHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit CustomHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~CustomHeaderView();

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;//重写绘制函数
    QSize sectionSizeFromContents(int logicalIndex) const override;//重写大小函数
};