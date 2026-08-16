// The "Performance" tab: processor, graphics card and memory as a bar
// with a rolling graph beside it.
//
// The layout is the original one - the bar with the instant value on the
// left, the history of the last two minutes on the right, both inside a
// labelled group. Only one row is new, one that did not exist in 1996:
// the graphics card.
#pragma once

#include <QWidget>

class QLabel;
class QLayout;

class Bar;
class GpuReader;
class History;

class PerformancePage : public QWidget {
    Q_OBJECT

public:
    explicit PerformancePage(QWidget *parent = nullptr);
    ~PerformancePage() override;

    // Fed by the main window once per tick. CPU load and memory are
    // already known there - reading them a second time would be double
    // work and would produce two slightly different numbers in the same
    // window.
    void setValues(double cpu, double memory, qint64 memoryUsedKb,
                   qint64 memoryTotalKb);

private:
    struct Row {
        Bar *bar = nullptr;
        History *graph = nullptr;
        QLabel *value = nullptr;
        QLabel *note = nullptr;
    };

    Row buildRow(const QString &title, const QString &graphTitle,
                 QWidget *parent, QLayout *layout);

    Row m_cpu;
    Row m_gpu;
    Row m_ram;
    GpuReader *m_gpuReader = nullptr;
};
