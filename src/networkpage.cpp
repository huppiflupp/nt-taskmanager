#include "networkpage.h"

#include "meters.h"

#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString rate(qint64 bytesPerSecond)
{
    const double kb = bytesPerSecond / 1024.0;
    if (kb < 1000.0) {
        return QStringLiteral("%1 KB/s").arg(kb, 0, 'f', 1);
    }
    return QStringLiteral("%1 MB/s").arg(kb / 1024.0, 0, 'f', 2);
}

} // namespace

NetworkPage::NetworkPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_graphs = new QVBoxLayout;
    m_graphs->setSpacing(6);
    layout->addLayout(m_graphs, 1);

    // A QTableWidget and not a model of its own, unlike processes and
    // services: two to four rows live here, nobody selects them and
    // nobody sorts them. A model would be three times the code for
    // nothing.
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Adapter Name"),
                                        QStringLiteral("State"),
                                        QStringLiteral("Utilization"),
                                        QStringLiteral("Link Speed"),
                                        QStringLiteral("Sent"),
                                        QStringLiteral("Received")});
    m_table->verticalHeader()->hide();
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setMaximumHeight(120);
    layout->addWidget(m_table, 0);
}

NetworkPage::Meter &NetworkPage::meterFor(const NetworkAdapter &adapter)
{
    auto hit = m_meters.find(adapter.name);
    if (hit != m_meters.end()) {
        return *hit;
    }

    // Created when the interface first appears. A USB adapter plugged in
    // while the program runs gets its graph without rebuilding the
    // window.
    Meter fresh;
    fresh.box = new QGroupBox(adapter.name, this);
    auto *inner = new QVBoxLayout(fresh.box);
    inner->setContentsMargins(6, 4, 6, 4);
    fresh.graph = new History(fresh.box);
    inner->addWidget(fresh.graph);
    m_graphs->addWidget(fresh.box);

    return *m_meters.insert(adapter.name, fresh);
}

void NetworkPage::refresh()
{
    const QVector<NetworkAdapter> adapters = m_reader.read();

    m_table->setRowCount(adapters.size());
    int row = 0;

    for (const NetworkAdapter &a : adapters) {
        Meter &meter = meterFor(a);

        double percent = 0.0;
        QString caption = QStringLiteral("%1 (%2)").arg(a.name, a.kindName());

        if (a.utilisation >= 0.0) {
            percent = a.utilisation;
        } else {
            // Without a known link speed there would be nothing to draw
            // and the graph would stay empty forever. So it is scaled to
            // the highest rate seen so far. That goes into the title,
            // because without the note a spike at 100 per cent looks
            // like a saturated link when it only means "more than ever
            // before".
            const double raw = qMax(a.received, a.sent);
            meter.peak = qMax(meter.peak, raw);
            if (meter.peak > 0.0) {
                percent = 100.0 * raw / meter.peak;
            }
            // Only label it once there is a peak. While nothing has
            // happened it would read "scaled to peak: 0.0 KB/s" - a
            // statement that says nothing and still looks like a
            // measurement.
            if (meter.peak > 0.0) {
                caption += QStringLiteral("  (scaled to peak: %1)")
                               .arg(rate(static_cast<qint64>(meter.peak)));
            }
        }

        meter.box->setTitle(caption);
        meter.graph->push(percent);

        auto set = [this, row](int column, const QString &text) {
            auto *cell = m_table->item(row, column);
            if (!cell) {
                cell = new QTableWidgetItem;
                m_table->setItem(row, column, cell);
            }
            cell->setText(text);
        };

        set(0, QStringLiteral("%1 (%2)").arg(a.name, a.kindName()));
        set(1, a.state);
        set(2, a.utilisation >= 0.0
                   ? QStringLiteral("%1 %").arg(a.utilisation, 0, 'f', 2)
                   : QStringLiteral("n/a"));
        set(3, a.speedMbit > 0 ? QStringLiteral("%1 Mbps").arg(a.speedMbit)
                               : QStringLiteral("unknown"));
        set(4, rate(a.sent));
        set(5, rate(a.received));
        ++row;
    }

    // Fit the columns once, not on every measurement: otherwise they
    // would twitch every second as soon as a rate goes from 9 to
    // 10 KB/s.
    if (!m_widthsSet && !adapters.isEmpty()) {
        m_table->resizeColumnsToContents();
        m_widthsSet = true;
    }
}
