// The "Networking" tab: one graph per interface, the table below it with
// utilisation, link speed and state.
#pragma once

#include <QHash>
#include <QWidget>

#include "networkreader.h"

class QGroupBox;
class QTableWidget;
class QVBoxLayout;

class History;

class NetworkPage : public QWidget {
    Q_OBJECT

public:
    explicit NetworkPage(QWidget *parent = nullptr);

    void refresh();

private:
    struct Meter {
        QGroupBox *box = nullptr;
        History *graph = nullptr;
        // Highest rate seen so far, for interfaces without a known link
        // speed.
        double peak = 0.0;
    };

    Meter &meterFor(const NetworkAdapter &adapter);

    NetworkReader m_reader;
    QHash<QString, Meter> m_meters;
    QVBoxLayout *m_graphs = nullptr;
    QTableWidget *m_table = nullptr;
    bool m_widthsSet = false;
};
