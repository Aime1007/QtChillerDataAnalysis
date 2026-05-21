#ifndef RANGESLIDER_H
#define RANGESLIDER_H

#include <QSlider>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QLinearGradient>

class RangeSliderPrivate;

class RangeSlider : public QSlider
{
    Q_OBJECT
        Q_PROPERTY(int minValue READ minValue WRITE setMinValue NOTIFY minValueChanged)
        Q_PROPERTY(int maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged)
        Q_PROPERTY(HandleMovementMode handleMovementMode READ handleMovementMode WRITE setHandleMovementMode NOTIFY handleMovementModeChanged)
        Q_ENUMS(HandleMovementMode)
        Q_ENUMS(SpanHandle)

public:
    enum SpanHandle {
        NoHandle,
        LowerHandle,
        UpperHandle
    };

    enum HandleMovementMode {
        FreeMovement,
        NoCrossing,
        NoOverlapping
    };

    explicit RangeSlider(QWidget* parent = nullptr);
    explicit RangeSlider(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~RangeSlider() override;

    // 核心接口（统一命名：min/max 对应 lower/upper）
    int minValue() const;          // 等价于 lowerValue
    void setMinValue(int value);   // 等价于 setLowerValue
    int maxValue() const;          // 等价于 upperValue
    void setMaxValue(int value);   // 等价于 setUpperValue
    void setSpan(int min, int max);

    // 兼容接口（可选，避免命名混淆）
    inline int lowerValue() const { return minValue(); }
    inline void setLowerValue(int value) { setMinValue(value); }
    inline int upperValue() const { return maxValue(); }
    inline void setUpperValue(int value) { setMaxValue(value); }

    HandleMovementMode handleMovementMode() const;
    void setHandleMovementMode(HandleMovementMode mode);

signals:
    void minValueChanged(int value);
    void maxValueChanged(int value);
    void spanChanged(int min, int max);
    void handleMovementModeChanged(HandleMovementMode mode);

private:
    // 兼容信号（可选，避免命名混淆）
    inline void lowerValueChanged(int value) { emit minValueChanged(value); }
    inline void upperValueChanged(int value) { emit maxValueChanged(value); }

protected:
    void setLowerPosition(int lower);
    void setUpperPosition(int upper);
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void init();
    RangeSliderPrivate* d_ptr;
    Q_DECLARE_PRIVATE(RangeSlider)
};

#endif // RANGESLIDER_H