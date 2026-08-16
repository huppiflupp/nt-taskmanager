// The two drawn widgets: a bar and a rolling graph.
//
// This is the one place where the program does draw for itself - no
// widget style provides bars or curves. The frame around them still
// comes from the style (QStyle::PE_Frame), so the recess matches every
// other table in the window.
//
// Green on black is deliberate and does NOT follow the colour scheme.
// Windows did the same: window colours followed the scheme, the meters
// in the task manager stayed green. Tinting them loses exactly the
// picture everyone recognises.
#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class Bar : public QWidget {
    Q_OBJECT

public:
    explicit Bar(QWidget *parent = nullptr);

    void setValue(double percent);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_value = 0.0;
};

class History : public QWidget {
    Q_OBJECT

public:
    explicit History(QWidget *parent = nullptr);

    void push(double percent);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // Fixed length, written as a ring. The graph always covers the same
    // span of time, no matter how wide the window happens to be -
    // otherwise dragging the edge would change what the curve means.
    static constexpr int Samples = 120;

    QVector<double> m_values;
    int m_next = 0;
    int m_filled = 0;
    // Counts samples so the grid travels left with the data. Without it
    // the curve appears frozen whenever the load stays flat.
    int m_offset = 0;
};
