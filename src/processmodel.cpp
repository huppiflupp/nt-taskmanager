#include "processmodel.h"

#include <QLocale>
#include <algorithm>

ProcessModel::ProcessModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int ProcessModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

const Process *ProcessModel::process(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return nullptr;
    }
    return &m_rows.at(row);
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    const Process *p = process(index.row());
    if (!p) {
        return {};
    }

    // Qt::UserRole carries the raw value. The view sorts by it - without
    // that it would sort by the label, and "9,000 K" would come after
    // "10,000 K".
    if (role == Qt::UserRole) {
        switch (index.column()) {
        case ColumnName:   return p->name;
        case ColumnUser:   return p->user;
        case ColumnCpu:    return p->cpu;
        case ColumnMemory: return p->memory;
        case ColumnPid:    return p->pid;
        }
        return {};
    }

    if (role == Qt::TextAlignmentRole) {
        // Numbers right, text left - as in the original.
        switch (index.column()) {
        case ColumnCpu:
        case ColumnMemory:
        case ColumnPid:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case ColumnName:
        return p->name;
    case ColumnUser:
        return p->user;
    case ColumnCpu:
        // Two digits with a leading zero, the way the original wrote it.
        return QStringLiteral("%1").arg(qRound(p->cpu), 2, 10, QLatin1Char('0'));
    case ColumnMemory:
        return QLocale().toString(p->memory) + QStringLiteral(" K");
    case ColumnPid:
        return p->pid;
    }
    return {};
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case ColumnName:   return QStringLiteral("Image Name");
    case ColumnUser:   return QStringLiteral("User Name");
    case ColumnCpu:    return QStringLiteral("CPU");
    case ColumnMemory: return QStringLiteral("Mem Usage");
    case ColumnPid:    return QStringLiteral("PID");
    }
    return {};
}

void ProcessModel::refresh(bool showForeign)
{
    QVector<Process> fresh = m_reader.read();
    if (!showForeign) {
        fresh.removeIf([](const Process &p) { return !p.own; });
    }
    // Order by pid so the sequence is stable between two readings.
    // Sorting for display happens in the view anyway.
    std::sort(fresh.begin(), fresh.end(),
              [](const Process &a, const Process &b) { return a.pid < b.pid; });

    // If these are the same processes as a moment ago, only the values
    // are swapped - selection, scroll position and sort order stay put.
    // Only when one appears or disappears is the view rebuilt; the
    // window remembers the selected pid for that case.
    bool sameSet = (fresh.size() == m_rows.size());
    if (sameSet) {
        for (int i = 0; i < fresh.size(); ++i) {
            if (fresh.at(i).pid != m_rows.at(i).pid) {
                sameSet = false;
                break;
            }
        }
    }

    if (sameSet) {
        m_rows = std::move(fresh);
        if (!m_rows.isEmpty()) {
            Q_EMIT dataChanged(
                index(0, 0),
                index(static_cast<int>(m_rows.size()) - 1, ColumnCount - 1));
        }
        return;
    }

    beginResetModel();
    m_rows = std::move(fresh);
    endResetModel();
}
