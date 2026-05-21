#include "RangeSlider.h"
#include "RangeSlider_p.h"
#include <QMouseEvent>
#include <QStylePainter>
#include <QStyleOptionSlider>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QDebug> // 新增：调试日志

// ========== 私有实现类 RangeSliderPrivate ==========
RangeSliderPrivate::RangeSliderPrivate(RangeSlider* q) : q_ptr(q)
{
    fillGradient.setColorAt(0, QColor(231, 80, 229));
    fillGradient.setColorAt(1, QColor(7, 208, 255));
}

void RangeSliderPrivate::initStyleOption(QStyleOptionSlider* opt, RangeSlider::SpanHandle handle) const
{
    const RangeSlider* q = q_ptr;
    q->initStyleOption(opt);
    opt->sliderPosition = (handle == RangeSlider::LowerHandle) ? lowerPos : upperPos;
    opt->sliderValue = (handle == RangeSlider::LowerHandle) ? lower : upper;
}

int RangeSliderPrivate::pick(const QPoint& pt) const
{
    return q_ptr->orientation() == Qt::Horizontal ? pt.x() : pt.y();
}

int RangeSliderPrivate::pixelPosToRangeValue(int pos) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    const QSlider* q = q_ptr;
    const QRect gr = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, q);
    const QRect sr = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, q);

    int sliderMin = 0, sliderMax = 0, sliderLength = 0;
    if (q->orientation() == Qt::Horizontal) {
        sliderLength = sr.width();
        sliderMin = gr.x();
        sliderMax = gr.right() - sliderLength + 1;
    }
    else {
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;
    }

    return QStyle::sliderValueFromPosition(q->minimum(), q->maximum(), pos - sliderMin,
        sliderMax - sliderMin, opt.upsideDown);
}

void RangeSliderPrivate::handleMousePress(const QPoint& pos, QStyle::SubControl& control, int value, RangeSlider::SpanHandle handle)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt, handle);
    RangeSlider* q = q_ptr;

    const QStyle::SubControl oldControl = control;
    control = q->style()->hitTestComplexControl(QStyle::CC_Slider, &opt, pos, q);
    const QRect sr = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, q);

    if (control == QStyle::SC_SliderHandle) {
        position = value;
        offset = pick(pos - sr.topLeft());
        lastPressed = handle;
        q->setSliderDown(true);
    }
    if (control != oldControl)
        q->update(sr);
}

void RangeSliderPrivate::drawSpan(QStylePainter* painter, const QRect& rect)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const RangeSlider* q = q_ptr;

    QRect groove = q->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, q);
    if (opt.orientation == Qt::Horizontal)
        groove.adjust(0, 0, -1, 0);
    else
        groove.adjust(0, 0, 0, -1);

    QRect rt = rect.intersected(groove);
    rt.adjust(0, 0, 1, 1);

    if (opt.orientation == Qt::Horizontal) {
        fillGradient.setStart(rt.left(), rt.top());
        fillGradient.setFinalStop(rt.right(), rt.top());
    }
    else {
        fillGradient.setStart(rt.left(), rt.top());
        fillGradient.setFinalStop(rt.left(), rt.bottom());
    }

    painter->setBrush(fillGradient);
    painter->setPen(Qt::transparent);
    painter->drawRoundedRect(rt, 3, 3);
}

void RangeSliderPrivate::drawHandle(QStylePainter* painter, RangeSlider::SpanHandle handle) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt, handle);
    opt.subControls = QStyle::SC_SliderHandle;

    QStyle::SubControl pressed = (handle == RangeSlider::LowerHandle) ? lowerPressed : upperPressed;
    if (pressed == QStyle::SC_SliderHandle) {
        opt.activeSubControls = pressed;
        opt.state |= QStyle::State_Sunken;
    }
    painter->drawComplexControl(QStyle::CC_Slider, opt);
}

void RangeSliderPrivate::triggerAction(QAbstractSlider::SliderAction action, bool main)
{
    int value = 0;
    bool no = false;
    bool up = false;
    const int min = q_ptr->minimum();
    const int max = q_ptr->maximum();
    const RangeSlider::SpanHandle altControl = (mainControl == RangeSlider::LowerHandle) ? RangeSlider::UpperHandle : RangeSlider::LowerHandle;

    blockTracking = true;

    switch (action) {
    case QAbstractSlider::SliderSingleStepAdd:
        if ((main && mainControl == RangeSlider::UpperHandle) || (!main && altControl == RangeSlider::UpperHandle)) {
            value = qBound(min, upper + q_ptr->singleStep(), max);
            up = true;
            break;
        }
        value = qBound(min, lower + q_ptr->singleStep(), max);
        break;
    case QAbstractSlider::SliderSingleStepSub:
        if ((main && mainControl == RangeSlider::UpperHandle) || (!main && altControl == RangeSlider::UpperHandle)) {
            value = qBound(min, upper - q_ptr->singleStep(), max);
            up = true;
            break;
        }
        value = qBound(min, lower - q_ptr->singleStep(), max);
        break;
    case QAbstractSlider::SliderToMinimum:
        value = min;
        up = (main && mainControl == RangeSlider::UpperHandle) || (!main && altControl == RangeSlider::UpperHandle);
        break;
    case QAbstractSlider::SliderToMaximum:
        value = max;
        up = (main && mainControl == RangeSlider::UpperHandle) || (!main && altControl == RangeSlider::UpperHandle);
        break;
    case QAbstractSlider::SliderMove:
        up = (main && mainControl == RangeSlider::UpperHandle) || (!main && altControl == RangeSlider::UpperHandle);
    case QAbstractSlider::SliderNoAction:
        no = true;
        break;
    default:
        break;
    }

    if (!no && !up) {
        if (movement == RangeSlider::NoCrossing)
            value = qMin(value, upper);
        else if (movement == RangeSlider::NoOverlapping)
            value = qMin(value, upper - 1);

        if (movement == RangeSlider::FreeMovement && value > upper) {
            swapControls();
            q_ptr->setUpperPosition(value);
        }
        else {
            q_ptr->setLowerPosition(value);
        }
    }
    else if (!no) {
        if (movement == RangeSlider::NoCrossing)
            value = qMax(value, lower);
        else if (movement == RangeSlider::NoOverlapping)
            value = qMax(value, lower + 1);

        if (movement == RangeSlider::FreeMovement && value < lower) {
            swapControls();
            q_ptr->setLowerPosition(value);
        }
        else {
            q_ptr->setUpperPosition(value);
        }
    }

    blockTracking = false;

    q_ptr->setMinValue(lowerPos);
    q_ptr->setMaxValue(upperPos);
}

void RangeSliderPrivate::swapControls()
{
    qSwap(lower, upper);
    qSwap(lowerPressed, upperPressed);
    lastPressed = (lastPressed == RangeSlider::LowerHandle) ? RangeSlider::UpperHandle : RangeSlider::LowerHandle;
    mainControl = (mainControl == RangeSlider::LowerHandle) ? RangeSlider::UpperHandle : RangeSlider::LowerHandle;
}

// ========== 公有类 RangeSlider ==========
RangeSlider::RangeSlider(QWidget* parent) : QSlider(parent), d_ptr(new RangeSliderPrivate(this))
{
    init();
}

RangeSlider::RangeSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent), d_ptr(new RangeSliderPrivate(this))
{
    init();
}

RangeSlider::~RangeSlider()
{
    delete d_ptr;
}

void RangeSlider::init()
{
    setStyleSheet(R"(
        QSlider::sub-page:horizontal { background: transparent; }
        QSlider::sub-page:vertical { background: transparent; }
        QSlider::tick:horizontal {
            background: #666;
            width: 1px;
            height: 8px;
            margin: 0 -1px;
        }
        QSlider::groove:horizontal {
            border: 1px solid #bbb;
            background: #eee;
            height: 8px;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #eee, stop:1 #ccc);
            border: 1px solid #777;
            width: 18px;
            height: 18px;
            margin: -5px 0;
            border-radius: 9px;
        }
    )");
    connect(this, &QSlider::rangeChanged, this, [=](int min, int max) {
        Q_UNUSED(min); Q_UNUSED(max);
        Q_D(RangeSlider);
        d->lower = qBound(this->minimum(), d->lower, this->maximum());
        d->upper = qBound(this->minimum(), d->upper, this->maximum());
        // 范围变化时强制设置默认刻度间隔
        if (this->tickInterval() <= 0 && (max - min) > 0) {
            this->setTickInterval((max - min) / 10); // 分成10等份
        }
        });
}

int RangeSlider::minValue() const
{
    Q_D(const RangeSlider);
    return qMin(d->lower, d->upper);
}

void RangeSlider::setMinValue(int value)
{
    Q_D(RangeSlider);
    setSpan(value, d->upper);
}

int RangeSlider::maxValue() const
{
    Q_D(const RangeSlider);
    return qMax(d->lower, d->upper);
}

void RangeSlider::setMaxValue(int value)
{
    Q_D(RangeSlider);
    setSpan(d->lower, value);
}

void RangeSlider::setSpan(int min, int max)
{
    Q_D(RangeSlider);
    const int low = qBound(minimum(), qMin(min, max), maximum());
    const int upp = qBound(minimum(), qMax(min, max), maximum());

    bool minChanged = (low != d->lower);
    bool maxChanged = (upp != d->upper);

    if (minChanged) {
        d->lower = low;
        d->lowerPos = low;
        emit minValueChanged(low);
    }
    if (maxChanged) {
        d->upper = upp;
        d->upperPos = upp;
        emit maxValueChanged(upp);
    }
    if (minChanged || maxChanged) {
        emit spanChanged(d->lower, d->upper);
        update();
    }
}

RangeSlider::HandleMovementMode RangeSlider::handleMovementMode() const
{
    Q_D(const RangeSlider);
    return d->movement;
}

void RangeSlider::setHandleMovementMode(HandleMovementMode mode)
{
    Q_D(RangeSlider);
    if (d->movement == mode) return;
    d->movement = mode;
    emit handleMovementModeChanged(mode);
    update();
}

void RangeSlider::setLowerPosition(int lower)
{
    Q_D(RangeSlider);
    if (d->lowerPos == lower) return;
    d->lowerPos = lower;
    if (isSliderDown())
        emit minValueChanged(lower);
    if (hasTracking() && !d->blockTracking) {
        d->triggerAction(QAbstractSlider::SliderMove, (d->mainControl == RangeSlider::LowerHandle));
    }
}

void RangeSlider::setUpperPosition(int upper)
{
    Q_D(RangeSlider);
    if (d->upperPos == upper) return;
    d->upperPos = upper;
    if (isSliderDown())
        emit maxValueChanged(upper);
    if (hasTracking() && !d->blockTracking) {
        d->triggerAction(QAbstractSlider::SliderMove, (d->mainControl == RangeSlider::UpperHandle));
    }
}

void RangeSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStylePainter painter(this);
    Q_D(RangeSlider);

    // ========== 第一步：优先绘制刻度（独立绘制，避免被覆盖） ==========
    QStyleOptionSlider tickOpt;
    this->initStyleOption(&tickOpt);
    // 只绘制刻度和轨道，不绘制滑块
    tickOpt.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderTickmarks;
    tickOpt.tickPosition = this->tickPosition();
    tickOpt.tickInterval = this->tickInterval();
    // 强制启用刻度绘制
    tickOpt.state |= QStyle::State_Enabled;
    painter.drawComplexControl(QStyle::CC_Slider, tickOpt);

    // ========== 第二步：绘制刻度下方的数值文本 ==========
    if (tickPosition() == QSlider::TicksBelow && orientation() == Qt::Horizontal) {
        // 1. 获取轨道区域（用于计算刻度位置）
        QRect grooveRect = style()->subControlRect(QStyle::CC_Slider, &tickOpt, QStyle::SC_SliderGroove, this);
        int sliderMin = grooveRect.x();
        int sliderMax = grooveRect.right();
        int totalRange = maximum() - minimum();

        int tickInterval = this->tickInterval();

        // 只有totalRange>0时才绘制（避免除以0）
        if (totalRange <= 0) return;
/*
        // 3. 遍历所有刻度位置，绘制数值
        for (int value = minimum(); value <= maximum(); value += tickInterval) {
            // 数值转像素位置（适配滑块范围）
            int pixelPos = sliderMin + ((value - minimum()) * (sliderMax - sliderMin)) / totalRange;
            // 手动绘制刻度线（轨道下方，核心修改）
            painter.setPen(QPen(QColor(33, 33, 33), 2)); // 深色、粗线
            // 起点：(像素位置, 轨道底部)，终点：(像素位置, 轨道底部 + 10)
            painter.drawLine(pixelPos, grooveRect.bottom() + 2, pixelPos, grooveRect.bottom() + 8); // 刻度线
            // 文本绘制参数
            QString text = QString::number(value); // 刻度对应的数值
            QFont font = painter.font();
            font.setPointSize(6); // 数值字体大小
            painter.setFont(font);

            // 文本坐标：刻度正下方，居中对齐（避免重叠）
            QRect textRect(pixelPos - 15, grooveRect.bottom() + 10, 30, 15); // 文本区域
            painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, text);
        }
*/
        // 3. 遍历所有刻度位置，绘制数值
        for (int value = minimum(); value <= maximum(); value += tickInterval) {
            // 数值转像素位置（适配滑块范围）
            int pixelPos = sliderMin + ((value - minimum()) * (sliderMax - sliderMin)) / totalRange;

            // 手动绘制刻度线
            painter.setPen(QPen(QColor(33, 33, 33), 2));
            painter.drawLine(pixelPos, grooveRect.bottom() + 2, pixelPos, grooveRect.bottom() + 8);

            // 文本绘制参数
            QString text = QString::number(value);
            QFont font = painter.font();
            font.setPointSize(6);
            painter.setFont(font);

            // 动态计算文本区域和对齐方式（核心修改部分）
            QRect textRect;
            int alignFlag = Qt::AlignTop;

            if (value == minimum()) {
                // 最左端：矩形从刻度线开始向右延伸，左对齐
                textRect = QRect(pixelPos, grooveRect.bottom() + 10, 40, 15);
                alignFlag |= Qt::AlignLeft;
            }
            else if (value == maximum()) {
                // 最右端：矩形从刻度线向左延伸，右对齐
                textRect = QRect(pixelPos - 40, grooveRect.bottom() + 10, 40, 15);
                alignFlag |= Qt::AlignRight;
            }
            else {
                // 中间：以刻度线为中心，居中对齐
                textRect = QRect(pixelPos - 20, grooveRect.bottom() + 10, 40, 15);
                alignFlag |= Qt::AlignHCenter;
            }

            // 绘制文本
            painter.drawText(textRect, alignFlag, text);
        }
    }

    // ========== 第三步：绘制双滑块渐变填充 ==========
    QStyleOptionSlider opt;
    d->initStyleOption(&opt);
    opt.sliderPosition = d->lowerPos;
    const QRect lr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    const int lrv = d->pick(lr.center());

    opt.sliderPosition = d->upperPos;
    const QRect ur = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    const int urv = d->pick(ur.center());

    const int minv = qMin(lrv, urv);
    const int maxv = qMax(lrv, urv);
    const QPoint c = QRect(lr.center(), ur.center()).center();
    QRect spanRect;

    if (orientation() == Qt::Horizontal)
        spanRect = QRect(QPoint(minv, c.y() - 2), QPoint(maxv, c.y() + 1));
    else
        spanRect = QRect(QPoint(c.x() - 2, minv), QPoint(c.x() + 1, maxv));

    d->drawSpan(&painter, spanRect);

    // ========== 第四步：绘制双滑块 ==========
    switch (d->lastPressed) {
    case RangeSlider::LowerHandle:
        d->drawHandle(&painter, RangeSlider::UpperHandle);
        d->drawHandle(&painter, RangeSlider::LowerHandle);
        break;
    default:
        d->drawHandle(&painter, RangeSlider::LowerHandle);
        d->drawHandle(&painter, RangeSlider::UpperHandle);
        break;
    }
}

void RangeSlider::mousePressEvent(QMouseEvent* event)
{
    Q_D(RangeSlider);
    if (minimum() == maximum() || (event->buttons() ^ event->button())) {
        event->ignore();
        return;
    }

    d->handleMousePress(event->pos(), d->upperPressed, d->upper, RangeSlider::UpperHandle);
    if (d->upperPressed != QStyle::SC_SliderHandle)
        d->handleMousePress(event->pos(), d->lowerPressed, d->lower, RangeSlider::LowerHandle);

    d->firstMovement = true;
    event->accept();
}

void RangeSlider::mouseMoveEvent(QMouseEvent* event)
{
    Q_D(RangeSlider);
    if (d->lowerPressed != QStyle::SC_SliderHandle && d->upperPressed != QStyle::SC_SliderHandle) {
        event->ignore();
        return;
    }

    QStyleOptionSlider opt;
    d->initStyleOption(&opt);
    const int m = style()->pixelMetric(QStyle::PM_MaximumDragDistance, &opt, this);
    int newPosition = d->pixelPosToRangeValue(d->pick(event->pos()) - d->offset);

    if (m >= 0) {
        const QRect r = rect().adjusted(-m, -m, m, m);
        if (!r.contains(event->pos()))
            newPosition = d->position;
    }

    if (d->firstMovement) {
        if (d->lower == d->upper && newPosition < minValue()) {
            d->swapControls();
            d->firstMovement = false;
        }
        else {
            d->firstMovement = false;
        }
    }

    if (d->lowerPressed == QStyle::SC_SliderHandle) {
        if (d->movement == NoCrossing)
            newPosition = qMin(newPosition, maxValue());
        else if (d->movement == NoOverlapping)
            newPosition = qMin(newPosition, maxValue() - 1);

        if (d->movement == FreeMovement && newPosition > d->upper) {
            d->swapControls();
            setUpperPosition(newPosition);
        }
        else {
            setLowerPosition(newPosition);
        }
    }
    else if (d->upperPressed == QStyle::SC_SliderHandle) {
        if (d->movement == NoCrossing)
            newPosition = qMax(newPosition, minValue());
        else if (d->movement == NoOverlapping)
            newPosition = qMax(newPosition, minValue() + 1);

        if (d->movement == FreeMovement && newPosition < d->lower) {
            d->swapControls();
            setLowerPosition(newPosition);
        }
        else {
            setUpperPosition(newPosition);
        }
    }
    event->accept();
}

void RangeSlider::mouseReleaseEvent(QMouseEvent* event)
{
    Q_D(RangeSlider);
    QSlider::mouseReleaseEvent(event);
    setSliderDown(false);
    d->lowerPressed = QStyle::SC_None;
    d->upperPressed = QStyle::SC_None;
    update();
}

void RangeSlider::keyPressEvent(QKeyEvent* event)
{
    Q_D(RangeSlider);
    QSlider::keyPressEvent(event);

    bool main = true;
    QAbstractSlider::SliderAction action = QAbstractSlider::SliderNoAction;

    switch (event->key()) {
    case Qt::Key_Left:
        main = (orientation() == Qt::Horizontal);
        action = !invertedAppearance() ? QAbstractSlider::SliderSingleStepSub : QAbstractSlider::SliderSingleStepAdd;
        break;
    case Qt::Key_Right:
        main = (orientation() == Qt::Horizontal);
        action = !invertedAppearance() ? QAbstractSlider::SliderSingleStepAdd : QAbstractSlider::SliderSingleStepSub;
        break;
    case Qt::Key_Up:
        main = (orientation() == Qt::Vertical);
        action = invertedControls() ? QAbstractSlider::SliderSingleStepSub : QAbstractSlider::SliderSingleStepAdd;
        break;
    case Qt::Key_Down:
        main = (orientation() == Qt::Vertical);
        action = invertedControls() ? QAbstractSlider::SliderSingleStepAdd : QAbstractSlider::SliderSingleStepSub;
        break;
    case Qt::Key_Home:
        main = (d->mainControl == RangeSlider::LowerHandle);
        action = QAbstractSlider::SliderToMinimum;
        break;
    case Qt::Key_End:
        main = (d->mainControl == RangeSlider::UpperHandle);
        action = QAbstractSlider::SliderToMaximum;
        break;
    default:
        event->ignore();
        break;
    }

    if (action != QAbstractSlider::SliderNoAction)
        d->triggerAction(action, main);
}
