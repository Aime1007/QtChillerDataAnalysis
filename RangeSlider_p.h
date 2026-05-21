#ifndef RANGESLIDER_P_H
#define RANGESLIDER_P_H

#include "RangeSlider.h"
#include <QStylePainter>
#include <QStyleOptionSlider>
#include <QLinearGradient>

class RangeSliderPrivate
{
    Q_DECLARE_PUBLIC(RangeSlider)

public:
    RangeSliderPrivate(RangeSlider* q);
    ~RangeSliderPrivate() = default;

    void initStyleOption(QStyleOptionSlider* opt, RangeSlider::SpanHandle handle = RangeSlider::UpperHandle) const;
    int pick(const QPoint& pt) const;
    int pixelPosToRangeValue(int pos) const;
    void handleMousePress(const QPoint& pos, QStyle::SubControl& control, int value, RangeSlider::SpanHandle handle);
    void drawSpan(QStylePainter* painter, const QRect& rect); // 移除const
    void drawHandle(QStylePainter* painter, RangeSlider::SpanHandle handle) const;
    void triggerAction(QAbstractSlider::SliderAction action, bool main);
    void swapControls();

    int lower = 20;
    int upper = 80;
    int lowerPos = 20;
    int upperPos = 80;
    int offset = 0;
    int position = 0;

    RangeSlider::SpanHandle lastPressed = RangeSlider::NoHandle;
    RangeSlider::SpanHandle mainControl = RangeSlider::LowerHandle;
    QStyle::SubControl lowerPressed = QStyle::SC_None;
    QStyle::SubControl upperPressed = QStyle::SC_None;
    RangeSlider::HandleMovementMode movement = RangeSlider::NoCrossing;
    bool firstMovement = false;
    bool blockTracking = false;

    QLinearGradient fillGradient; // 渐变对象（非const）

private:
    RangeSlider* q_ptr;
};

#endif // RANGESLIDER_P_H