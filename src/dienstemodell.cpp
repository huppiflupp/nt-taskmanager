#include "dienstemodell.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

#include <algorithm>

namespace {
constexpr auto Dienstname = "org.freedesktop.systemd1";
constexpr auto Objektpfad = "/org/freedesktop/systemd1";
constexpr auto Schnittstelle = "org.freedesktop.systemd1.Manager";
}

Dienstemodell::Dienstemodell(QObject *eltern)
    : QAbstractTableModel(eltern)
{
}

int Dienstemodell::rowCount(const QModelIndex &eltern) const
{
    return eltern.isValid() ? 0 : static_cast<int>(m_zeilen.size());
}

int Dienstemodell::columnCount(const QModelIndex &eltern) const
{
    return eltern.isValid() ? 0 : SpaltenZahl;
}

const Dienst *Dienstemodell::dienst(int zeile) const
{
    if (zeile < 0 || zeile >= m_zeilen.size()) {
        return nullptr;
    }
    return &m_zeilen.at(zeile);
}

QVariant Dienstemodell::data(const QModelIndex &index, int rolle) const
{
    const Dienst *d = dienst(index.row());
    if (!d) {
        return {};
    }
    if (rolle != Qt::DisplayRole && rolle != Qt::UserRole) {
        return {};
    }
    switch (index.column()) {
    case SpalteName:         return d->name;
    case SpalteBeschreibung: return d->beschreibung;
    case SpalteAktiv:        return d->aktiv;
    case SpalteZustand:      return d->zustand;
    }
    return {};
}

QVariant Dienstemodell::headerData(int abschnitt, Qt::Orientation richtung,
                                   int rolle) const
{
    if (richtung != Qt::Horizontal || rolle != Qt::DisplayRole) {
        return {};
    }
    switch (abschnitt) {
    case SpalteName:         return QStringLiteral("Name");
    case SpalteBeschreibung: return QStringLiteral("Description");
    case SpalteAktiv:        return QStringLiteral("Status");
    case SpalteZustand:      return QStringLiteral("State");
    }
    return {};
}

void Dienstemodell::aktualisiere()
{
    m_fehler.clear();

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        m_fehler = QStringLiteral("System bus not available");
        beginResetModel();
        m_zeilen.clear();
        endResetModel();
        return;
    }

    QDBusMessage aufruf = QDBusMessage::createMethodCall(
        QLatin1String(Dienstname), QLatin1String(Objektpfad),
        QLatin1String(Schnittstelle), QStringLiteral("ListUnits"));
    const QDBusMessage antwort = bus.call(aufruf);
    if (antwort.type() == QDBusMessage::ErrorMessage) {
        m_fehler = antwort.errorMessage();
        beginResetModel();
        m_zeilen.clear();
        endResetModel();
        return;
    }

    // ListUnits liefert a(ssssssouso). Qt kennt diesen Verbund nicht von
    // sich aus, deshalb wird er Feld fuer Feld ausgelesen. Die
    // Reihenfolge steht in der Schnittstellenbeschreibung von systemd
    // und ist seit v208 unveraendert.
    QVector<Dienst> neu;
    const QDBusArgument feld = antwort.arguments().value(0).value<QDBusArgument>();
    feld.beginArray();
    while (!feld.atEnd()) {
        Dienst d;
        QString folgt;
        QDBusObjectPath pfad;
        uint auftrag = 0;
        QString auftragsart;
        QDBusObjectPath auftragspfad;

        feld.beginStructure();
        feld >> d.name >> d.beschreibung >> d.geladen >> d.aktiv >> d.zustand
             >> folgt >> pfad >> auftrag >> auftragsart >> auftragspfad;
        feld.endStructure();

        d.pfad = pfad.path();
        // Nur echte Dienste. ListUnits liefert auch Sockets, Ziele,
        // Einhaengepunkte und Zeitgeber - im Taskmanager stuenden dann
        // 400 Zeilen, von denen 300 keine Dienste sind.
        if (d.name.endsWith(QLatin1String(".service"))) {
            neu.append(d);
        }
    }
    feld.endArray();

    std::sort(neu.begin(), neu.end(), [](const Dienst &a, const Dienst &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    beginResetModel();
    m_zeilen = std::move(neu);
    endResetModel();
}
