#include "servicemodel.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

#include <algorithm>

namespace {
constexpr auto BusName = "org.freedesktop.systemd1";
constexpr auto ObjectPath = "/org/freedesktop/systemd1";
constexpr auto Interface = "org.freedesktop.systemd1.Manager";
}

ServiceModel::ServiceModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ServiceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int ServiceModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

const Service *ServiceModel::service(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return nullptr;
    }
    return &m_rows.at(row);
}

QVariant ServiceModel::data(const QModelIndex &index, int role) const
{
    const Service *s = service(index.row());
    if (!s) {
        return {};
    }
    if (role != Qt::DisplayRole && role != Qt::UserRole) {
        return {};
    }
    switch (index.column()) {
    case ColumnName:        return s->name;
    case ColumnDescription: return s->description;
    case ColumnActive:      return s->active;
    case ColumnState:       return s->state;
    }
    return {};
}

QVariant ServiceModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case ColumnName:        return QStringLiteral("Name");
    case ColumnDescription: return QStringLiteral("Description");
    case ColumnActive:      return QStringLiteral("Status");
    case ColumnState:       return QStringLiteral("State");
    }
    return {};
}

void ServiceModel::refresh()
{
    m_error.clear();

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        m_error = QStringLiteral("System bus not available");
        beginResetModel();
        m_rows.clear();
        endResetModel();
        return;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        QLatin1String(BusName), QLatin1String(ObjectPath),
        QLatin1String(Interface), QStringLiteral("ListUnits"));
    const QDBusMessage answer = bus.call(call);
    if (answer.type() == QDBusMessage::ErrorMessage) {
        m_error = answer.errorMessage();
        beginResetModel();
        m_rows.clear();
        endResetModel();
        return;
    }

    // ListUnits returns a(ssssssouso). Qt does not know that struct on
    // its own, so it is read field by field. The order is part of
    // systemd's interface description and has not changed since v208.
    QVector<Service> fresh;
    const QDBusArgument field = answer.arguments().value(0).value<QDBusArgument>();
    field.beginArray();
    while (!field.atEnd()) {
        Service s;
        QString following;
        QDBusObjectPath path;
        uint job = 0;
        QString jobType;
        QDBusObjectPath jobPath;

        field.beginStructure();
        field >> s.name >> s.description >> s.loaded >> s.active >> s.state
              >> following >> path >> job >> jobType >> jobPath;
        field.endStructure();

        s.path = path.path();
        // Real services only. ListUnits also returns sockets, targets,
        // mounts and timers - the tab would show 400 rows, 300 of which
        // are not services.
        if (s.name.endsWith(QLatin1String(".service"))) {
            fresh.append(s);
        }
    }
    field.endArray();

    std::sort(fresh.begin(), fresh.end(), [](const Service &a, const Service &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    beginResetModel();
    m_rows = std::move(fresh);
    endResetModel();
}
