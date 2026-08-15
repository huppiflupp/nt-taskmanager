#include "netzseite.h"

#include "anzeigen.h"

#include <QGroupBox>
#include <QHeaderView>
#include <QLocale>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString rate(qint64 bytes_je_sekunde)
{
    const double kb = bytes_je_sekunde / 1024.0;
    if (kb < 1000.0) {
        return QStringLiteral("%1 KB/s").arg(kb, 0, 'f', 1);
    }
    return QStringLiteral("%1 MB/s").arg(kb / 1024.0, 0, 'f', 2);
}

} // namespace

Netzseite::Netzseite(QWidget *eltern)
    : QWidget(eltern)
{
    auto *lage = new QVBoxLayout(this);
    lage->setContentsMargins(6, 6, 6, 6);
    lage->setSpacing(6);

    m_graphen = new QVBoxLayout;
    m_graphen->setSpacing(6);
    lage->addLayout(m_graphen, 1);

    // Ein QTableWidget und kein eigenes Modell, anders als bei Prozessen
    // und Diensten: Hier stehen zwei bis vier Zeilen, die niemand
    // auswaehlt und niemand sortiert. Ein Modell waere dreimal so viel
    // Code fuer nichts.
    m_tabelle = new QTableWidget(0, 6, this);
    m_tabelle->setHorizontalHeaderLabels({QStringLiteral("Adapter Name"),
                                          QStringLiteral("State"),
                                          QStringLiteral("Utilization"),
                                          QStringLiteral("Link Speed"),
                                          QStringLiteral("Sent"),
                                          QStringLiteral("Received")});
    m_tabelle->verticalHeader()->hide();
    m_tabelle->setShowGrid(false);
    m_tabelle->setSelectionMode(QAbstractItemView::NoSelection);
    m_tabelle->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabelle->horizontalHeader()->setStretchLastSection(true);
    m_tabelle->setMaximumHeight(120);
    lage->addWidget(m_tabelle, 0);
}

Netzseite::Anzeige &Netzseite::anzeigeFuer(const Netzadapter &adapter)
{
    auto treffer = m_anzeigen.find(adapter.name);
    if (treffer != m_anzeigen.end()) {
        return *treffer;
    }

    // Erst anlegen, wenn die Schnittstelle das erste Mal auftaucht. Ein
    // USB-Adapter, der waehrend des Laufs eingesteckt wird, bekommt so
    // seinen Verlauf, ohne dass das Fenster neu aufgebaut werden muss.
    Anzeige neu;
    neu.rahmen = new QGroupBox(adapter.name, this);
    auto *innen = new QVBoxLayout(neu.rahmen);
    innen->setContentsMargins(6, 4, 6, 4);
    neu.verlauf = new Verlauf(neu.rahmen);
    innen->addWidget(neu.verlauf);
    m_graphen->addWidget(neu.rahmen);

    return *m_anzeigen.insert(adapter.name, neu);
}

void Netzseite::aktualisiere()
{
    const QVector<Netzadapter> adapter = m_quelle.lies();

    m_tabelle->setRowCount(adapter.size());
    int zeile = 0;

    for (const Netzadapter &a : adapter) {
        Anzeige &anzeige = anzeigeFuer(a);

        double prozent = 0.0;
        QString beschriftung =
            QStringLiteral("%1 (%2)").arg(a.name, a.artname());

        if (a.auslastung >= 0.0) {
            prozent = a.auslastung;
        } else {
            // Ohne bekannte Linkgeschwindigkeit gaebe es nichts zu
            // zeichnen - der Verlauf bliebe fuer immer leer. Deshalb
            // wird hier auf die hoechste bisher gesehene Rate skaliert.
            // Das steht so auch im Titel, denn ohne den Hinweis sieht
            // eine Spitze bei 100 Prozent nach einer ausgelasteten
            // Leitung aus, und sie heisst nur "so viel wie noch nie".
            const double roh = qMax(a.empfangen, a.gesendet);
            anzeige.spitze = qMax(anzeige.spitze, roh);
            if (anzeige.spitze > 0.0) {
                prozent = 100.0 * roh / anzeige.spitze;
            }
            // Erst beschriften, wenn es einen Hoechstwert gibt. Solange
            // nichts lief, stuende dort "scaled to peak: 0.0 KB/s" -
            // eine Angabe, die nichts sagt und trotzdem nach Messung
            // aussieht.
            if (anzeige.spitze > 0.0) {
                beschriftung += QStringLiteral("  (scaled to peak: %1)")
                                    .arg(rate(static_cast<qint64>(anzeige.spitze)));
            }
        }

        anzeige.rahmen->setTitle(beschriftung);
        anzeige.verlauf->schiebe(prozent);

        auto setze = [this, zeile](int spalte, const QString &text) {
            auto *feld = m_tabelle->item(zeile, spalte);
            if (!feld) {
                feld = new QTableWidgetItem;
                m_tabelle->setItem(zeile, spalte, feld);
            }
            feld->setText(text);
        };

        setze(0, QStringLiteral("%1 (%2)").arg(a.name, a.artname()));
        setze(1, a.zustand);
        setze(2, a.auslastung >= 0.0
                     ? QStringLiteral("%1 %").arg(a.auslastung, 0, 'f', 2)
                     : QStringLiteral("n/a"));
        setze(3, a.tempo_mbit > 0
                     ? QStringLiteral("%1 Mbps").arg(a.tempo_mbit)
                     : QStringLiteral("unknown"));
        setze(4, rate(a.gesendet));
        setze(5, rate(a.empfangen));
        ++zeile;
    }

    // Einmal an den Inhalt anpassen, nicht bei jeder Messung: Sonst
    // zuckten die Spalten im Sekundentakt, sobald eine Rate von 9 auf
    // 10 KB/s springt.
    if (!m_breiten_gesetzt && !adapter.isEmpty()) {
        m_tabelle->resizeColumnsToContents();
        m_breiten_gesetzt = true;
    }
}
