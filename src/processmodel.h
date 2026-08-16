// The process table as a Qt model.
//
// Why a model and not a QTreeWidget filled with items: the list is read
// again every second. A widget tree would be cleared and refilled each
// time - selection gone, scroll position gone, sort order gone. A model
// only swaps the data and reports what changed; the view keeps its
// state.
#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "processreader.h"

class ProcessModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColumnName = 0,
        ColumnUser,
        ColumnCpu,
        ColumnMemory,
        ColumnPid,
        ColumnCount
    };

    explicit ProcessModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    // Read again. showForeign decides whether processes of other users
    // appear - the checkbox at the bottom of the window.
    void refresh(bool showForeign);

    const Process *process(int row) const;
    double totalCpu() const { return m_reader.totalCpu(); }
    double memoryUsage() const { return m_reader.memoryUsage(); }
    qint64 memoryUsed() const { return m_reader.memoryUsed(); }
    qint64 memoryTotal() const { return m_reader.memoryTotal(); }
    int count() const { return static_cast<int>(m_rows.size()); }

private:
    ProcessReader m_reader;
    QVector<Process> m_rows;
};
