// The systemd units as a Qt model.
//
// The Windows task manager listed the services of the system under
// "Services". The Linux equivalent are systemd units, and they are not
// fetched by running a command line tool: systemctl speaks D-Bus itself,
// so we do the same directly. That saves parsing output whose shape can
// change with any release.
#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct Service {
    QString name;        // sshd.service
    QString description; // "OpenSSH server daemon"
    QString loaded;      // loaded, not-found, masked
    QString active;      // active, inactive, failed
    QString state;       // running, exited, dead
    QString path;        // object path, for starting and stopping
};

class ServiceModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColumnName = 0,
        ColumnDescription,
        ColumnActive,
        ColumnState,
        ColumnCount
    };

    explicit ServiceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    void refresh();
    const Service *service(int row) const;
    int count() const { return static_cast<int>(m_rows.size()); }

    // Returns a message when the system bus cannot be reached, so it can
    // go into the status bar instead of leaving an empty table
    // unexplained.
    QString error() const { return m_error; }

private:
    QVector<Service> m_rows;
    QString m_error;
};
