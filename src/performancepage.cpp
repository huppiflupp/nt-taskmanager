#include "performancepage.h"

#include "gpureader.h"
#include "meters.h"

#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

#include <unistd.h>

namespace {
// The name of the processor as /proc/cpuinfo carries it. Every core
// block holds the same one - the first will do.
//
// The vendor decorations are dropped: "Intel(R) Core(TM) Ultra 5 245K"
// becomes "Intel Core Ultra 5 245K". The original said no more at this
// spot than one would say to a person.
QString processorName()
{
    QFile file(QStringLiteral("/proc/cpuinfo"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    for (const QByteArray &line : file.readAll().split('\n')) {
        if (!line.startsWith("model name")) {
            continue;
        }
        const int colon = line.indexOf(':');
        if (colon < 0) {
            break;
        }
        QString name = QString::fromLatin1(line.mid(colon + 1)).trimmed();
        name.remove(QStringLiteral("(R)"));
        name.remove(QStringLiteral("(TM)"));
        name.remove(QStringLiteral("(tm)"));
        return name.simplified();
    }
    // On ARM there is no model name. Better nothing than an invented
    // designation.
    return {};
}
} // namespace

PerformancePage::PerformancePage(QWidget *parent)
    : QWidget(parent)
    , m_gpuReader(new GpuReader)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_cpu = buildRow(QStringLiteral("CPU Usage"),
                     QStringLiteral("CPU Usage History"), this, layout);
    const QString processor = processorName();
    const int cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (!processor.isEmpty()) {
        m_cpu.note->setText(cores > 1 ? QStringLiteral("%1  %2 cores")
                                            .arg(processor)
                                            .arg(cores)
                                      : processor);
    }

    const QString gpuTitle = m_gpuReader->present()
                                 ? QStringLiteral("GPU Usage")
                                 : QStringLiteral("GPU Usage (no adapter)");
    m_gpu = buildRow(gpuTitle, QStringLiteral("GPU Usage History"), this, layout);
    if (m_gpuReader->present()) {
        m_gpu.note->setText(m_gpuReader->name());
    } else {
        // No bar stuck at zero and no curve pretending to be a
        // measurement - honestly empty instead.
        m_gpu.bar->setEnabled(false);
        m_gpu.graph->setEnabled(false);
        m_gpu.note->setText(QStringLiteral("none detected"));
    }

    m_ram = buildRow(QStringLiteral("Mem Usage"),
                     QStringLiteral("Memory Usage History"), this, layout);
}

PerformancePage::~PerformancePage()
{
    delete m_gpuReader;
}

PerformancePage::Row PerformancePage::buildRow(const QString &title,
                                               const QString &graphTitle,
                                               QWidget *parent, QLayout *layout)
{
    Row row;

    auto *line = new QHBoxLayout;
    line->setSpacing(6);

    auto *leftBox = new QGroupBox(title, parent);
    auto *left = new QVBoxLayout(leftBox);
    left->setContentsMargins(6, 4, 6, 4);
    row.bar = new Bar(leftBox);
    left->addWidget(row.bar, 1, Qt::AlignHCenter);
    row.value = new QLabel(QStringLiteral("0%"), leftBox);
    row.value->setAlignment(Qt::AlignCenter);
    left->addWidget(row.value);
    line->addWidget(leftBox, 0);

    auto *rightBox = new QGroupBox(graphTitle, parent);
    auto *right = new QVBoxLayout(rightBox);
    right->setContentsMargins(6, 4, 6, 4);
    row.graph = new History(rightBox);
    right->addWidget(row.graph, 1);
    row.note = new QLabel(rightBox);
    row.note->setAlignment(Qt::AlignRight);
    right->addWidget(row.note);
    line->addWidget(rightBox, 1);

    layout->addItem(line);
    return row;
}

void PerformancePage::setValues(double cpu, double memory,
                                qint64 memoryUsedKb, qint64 memoryTotalKb)
{
    m_cpu.bar->setValue(cpu);
    m_cpu.graph->push(cpu);
    m_cpu.value->setText(QStringLiteral("%1%").arg(qRound(cpu)));

    if (m_gpuReader->present()) {
        m_gpuReader->measure();
        const double load = m_gpuReader->load();
        m_gpu.bar->setValue(load);
        m_gpu.graph->push(load);
        m_gpu.value->setText(QStringLiteral("%1%").arg(qRound(load)));
        if (m_gpuReader->memoryTotal() > 0) {
            m_gpu.note->setText(
                QStringLiteral("%1  %2 / %3 MB")
                    .arg(m_gpuReader->name())
                    .arg(QLocale().toString(m_gpuReader->memoryUsed() / 1024))
                    .arg(QLocale().toString(m_gpuReader->memoryTotal() / 1024)));
        }
    }

    m_ram.bar->setValue(memory);
    m_ram.graph->push(memory);
    m_ram.value->setText(QStringLiteral("%1%").arg(qRound(memory)));
    if (memoryTotalKb > 0) {
        m_ram.note->setText(
            QStringLiteral("%1 / %2 MB")
                .arg(QLocale().toString(memoryUsedKb / 1024))
                .arg(QLocale().toString(memoryTotalKb / 1024)));
    }
}
