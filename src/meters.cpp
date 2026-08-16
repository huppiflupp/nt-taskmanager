#include "meters.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include <cmath>

namespace {
// The colours of the original. Not taken from the palette - see the
// comment in the header.
const QColor Background(0, 0, 0);
// Clearly darker than the curve: the grid should hint at the scale, not
// compete with the measurement. At (0,100,0) the field read as a green
// surface and the curve disappeared into it.
const QColor Grid(0, 60, 0);
const QColor Curve(0, 255, 0);
const QColor Segment(0, 220, 0);

// One segment of the bar: two pixels tall, one pixel of gap. That is how
// the bar looked in NT, and the reason it is not simply a filled
// rectangle.
constexpr int SegmentHeight = 2;
constexpr int SegmentGap = 1;

// Edge length of one grid cell in the graph.
constexpr double CellWidth = 14.0;

void drawFrame(QPainter *painter, QWidget *widget)
{
    QStyleOptionFrame frame;
    frame.initFrom(widget);
    frame.frameShape = QFrame::Panel;
    frame.lineWidth = 1;
    frame.midLineWidth = 0;
    frame.state |= QStyle::State_Sunken;
    widget->style()->drawPrimitive(QStyle::PE_Frame, &frame, painter, widget);
}
} // namespace

// ── Bar ──────────────────────────────────────────────────────────────

Bar::Bar(QWidget *parent)
    : QWidget(parent)
{
}

void Bar::setValue(double percent)
{
    m_value = qBound(0.0, percent, 100.0);
    update();
}

QSize Bar::sizeHint() const
{
    return {28, 110};
}

QSize Bar::minimumSizeHint() const
{
    return {20, 60};
}

void Bar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    drawFrame(&painter, this);

    const QRect inner = rect().adjusted(2, 2, -2, -2);
    painter.fillRect(inner, Background);

    const int step = SegmentHeight + SegmentGap;
    const int segmentsTotal = inner.height() / step;
    const int segmentsLit = qRound(segmentsTotal * m_value / 100.0);

    for (int i = 0; i < segmentsLit; ++i) {
        // Fill from the bottom up.
        const int y = inner.bottom() - (i + 1) * step + SegmentGap;
        painter.fillRect(inner.left(), y, inner.width(), SegmentHeight, Segment);
    }
}

// ── History ──────────────────────────────────────────────────────────

History::History(QWidget *parent)
    : QWidget(parent)
{
    m_values.resize(Samples);
}

void History::push(double percent)
{
    m_values[m_next] = qBound(0.0, percent, 100.0);
    m_next = (m_next + 1) % Samples;
    if (m_filled < Samples) {
        ++m_filled;
    }
    m_offset = (m_offset + 1) % Samples;
    update();
}

QSize History::sizeHint() const
{
    return {180, 110};
}

QSize History::minimumSizeHint() const
{
    return {80, 60};
}

void History::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    drawFrame(&painter, this);

    const QRect inner = rect().adjusted(2, 2, -2, -2);
    if (inner.width() < 4 || inner.height() < 4) {
        return;
    }
    painter.fillRect(inner, Background);
    painter.setClipRect(inner);

    // Horizontal distance between two samples. The 120 values ALWAYS
    // fill the whole width, no matter how wide the window is.
    //
    // One sample per pixel column was the first attempt, and it was
    // wrong: in a field 330 pixels wide, 120 values would have covered
    // only the right third and the other two thirds would have stayed
    // empty forever. The graph shows a fixed span of time, not a fixed
    // number of pixels.
    const double step = double(inner.width() - 1) / (Samples - 1);

    // Grid. The vertical lines travel with the data, the horizontal ones
    // stay put - just like the original, where the time axis moves and
    // the percentage axis does not.
    painter.setPen(Grid);
    const double offset = std::fmod(m_offset * step, CellWidth);
    for (double x = inner.right() - offset; x > inner.left(); x -= CellWidth) {
        painter.drawLine(QPointF(x, inner.top()), QPointF(x, inner.bottom()));
    }
    for (int i = 1; i < 6; ++i) {
        const int y = inner.top() + inner.height() * i / 6;
        painter.drawLine(inner.left(), y, inner.right(), y);
    }

    if (m_filled < 2) {
        return;
    }

    // The curve runs right to left: the newest value sits at the right
    // edge.
    QPolygonF line;
    line.reserve(m_filled);
    for (int i = 0; i < m_filled; ++i) {
        // i = 0 is the newest sample.
        const int slot = (m_next - 1 - i + 2 * Samples) % Samples;
        const double value = m_values.at(slot);
        const double x = inner.right() - i * step;
        const double y = inner.bottom() - (inner.height() - 1) * value / 100.0;
        line << QPointF(x, y);
    }

    painter.setPen(QPen(Curve, 1));
    painter.drawPolyline(line);
}
