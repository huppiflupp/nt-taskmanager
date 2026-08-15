#include "prozessmodell.h"

#include <QLocale>
#include <algorithm>

Prozessmodell::Prozessmodell(QObject *eltern)
    : QAbstractTableModel(eltern)
{
}

int Prozessmodell::rowCount(const QModelIndex &eltern) const
{
    return eltern.isValid() ? 0 : static_cast<int>(m_zeilen.size());
}

int Prozessmodell::columnCount(const QModelIndex &eltern) const
{
    return eltern.isValid() ? 0 : SpaltenZahl;
}

const Prozess *Prozessmodell::prozess(int zeile) const
{
    if (zeile < 0 || zeile >= m_zeilen.size()) {
        return nullptr;
    }
    return &m_zeilen.at(zeile);
}

QVariant Prozessmodell::data(const QModelIndex &index, int rolle) const
{
    const Prozess *p = prozess(index.row());
    if (!p) {
        return {};
    }

    // Qt::UserRole traegt den Rohwert. Die Ansicht sortiert danach -
    // ohne das wuerde nach der Beschriftung sortiert, und "9.000 K"
    // stuende dann hinter "10.000 K".
    if (rolle == Qt::UserRole) {
        switch (index.column()) {
        case SpalteName:     return p->name;
        case SpalteBenutzer: return p->benutzer;
        case SpalteCpu:      return p->cpu;
        case SpalteSpeicher: return p->speicher;
        case SpaltePid:      return p->pid;
        }
        return {};
    }

    if (rolle == Qt::TextAlignmentRole) {
        // Zahlen rechts, Text links - wie im Original.
        switch (index.column()) {
        case SpalteCpu:
        case SpalteSpeicher:
        case SpaltePid:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (rolle != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case SpalteName:
        return p->name;
    case SpalteBenutzer:
        return p->benutzer;
    case SpalteCpu:
        // Zweistellig mit fuehrender Null, so steht es im Original.
        return QStringLiteral("%1").arg(qRound(p->cpu), 2, 10, QLatin1Char('0'));
    case SpalteSpeicher:
        return QLocale().toString(p->speicher) + QStringLiteral(" K");
    case SpaltePid:
        return p->pid;
    }
    return {};
}

QVariant Prozessmodell::headerData(int abschnitt, Qt::Orientation richtung,
                                   int rolle) const
{
    if (richtung != Qt::Horizontal || rolle != Qt::DisplayRole) {
        return {};
    }
    switch (abschnitt) {
    case SpalteName:     return QStringLiteral("Image Name");
    case SpalteBenutzer: return QStringLiteral("User Name");
    case SpalteCpu:      return QStringLiteral("CPU");
    case SpalteSpeicher: return QStringLiteral("Mem Usage");
    case SpaltePid:      return QStringLiteral("PID");
    }
    return {};
}

void Prozessmodell::aktualisiere(bool fremde_zeigen)
{
    QVector<Prozess> neu = m_quelle.lies();
    if (!fremde_zeigen) {
        neu.removeIf([](const Prozess &p) { return !p.eigen; });
    }
    // Nach PID ordnen, damit die Reihenfolge zwischen zwei Messungen
    // stabil ist. Sortiert wird ohnehin in der Ansicht.
    std::sort(neu.begin(), neu.end(),
              [](const Prozess &a, const Prozess &b) { return a.pid < b.pid; });

    // Sind es dieselben Prozesse wie eben, werden nur die Werte
    // ausgetauscht - Auswahl, Bildlauf und Sortierung bleiben stehen.
    // Erst wenn einer dazukommt oder verschwindet, wird die Ansicht neu
    // aufgebaut; das Fenster merkt sich dafuer die ausgewaehlte PID.
    bool gleiche_menge = (neu.size() == m_zeilen.size());
    if (gleiche_menge) {
        for (int i = 0; i < neu.size(); ++i) {
            if (neu.at(i).pid != m_zeilen.at(i).pid) {
                gleiche_menge = false;
                break;
            }
        }
    }

    if (gleiche_menge) {
        m_zeilen = std::move(neu);
        if (!m_zeilen.isEmpty()) {
            Q_EMIT dataChanged(index(0, 0),
                               index(static_cast<int>(m_zeilen.size()) - 1,
                                     SpaltenZahl - 1));
        }
        return;
    }

    beginResetModel();
    m_zeilen = std::move(neu);
    endResetModel();
}
