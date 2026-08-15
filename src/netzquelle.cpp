#include "netzquelle.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

#include <cstring>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

QString sysfsWert(const QString &schnittstelle, const QString &datei)
{
    QFile f(QStringLiteral("/sys/class/net/") + schnittstelle
            + QLatin1Char('/') + datei);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    // readAll, nicht readLine mit atEnd: sysfs meldet wie procfs die
    // Groesse 0.
    return QString::fromLatin1(f.readAll()).trimmed();
}

// ── Bluetooth ────────────────────────────────────────────────────────
//
// Die Werte kommen per ioctl aus dem Kern. Die Strukturen stehen in
// <bluetooth/hci.h>, und dieser Header gehoert zu bluez-libs-devel -
// eine Bauabhaengigkeit fuer zwei Zahlen waere zu viel verlangt.
// Deshalb sind sie hier deklariert. Es ist eine Kernel-ABI und damit
// stabil; gegengeprueft gegen hciconfig auf demselben Rechner:
// RX 2988490 und TX 30409, auf das Byte gleich.
constexpr int AfBluetooth = 31;
constexpr int BtprotoHci = 1;
// Nicht die ausgerechnete Zahl (0x800448d3), sondern das Makro: es
// setzt sie aus Richtung, Groesse und Nummer zusammen, und das gilt
// auch auf Architekturen, die die Bits anders anordnen.
constexpr unsigned long HciGetDevInfo = _IOR('H', 211, int);

struct HciStatistik {
    unsigned int err_rx, err_tx, cmd_tx, evt_rx, acl_tx, acl_rx;
    unsigned int sco_tx, sco_rx, byte_rx, byte_tx;
};

struct HciInfo {
    unsigned short dev_id;
    char name[8];
    unsigned char bdaddr[6];
    unsigned int flags;
    unsigned char type;
    unsigned char features[8];
    unsigned int pkt_type;
    unsigned int link_policy;
    unsigned int link_mode;
    unsigned short acl_mtu;
    unsigned short acl_pkts;
    unsigned short sco_mtu;
    unsigned short sco_pkts;
    HciStatistik stat;
};

constexpr unsigned int HciFlagUp = 1U << 0;

} // namespace

QString Netzadapter::artname() const
{
    switch (art) {
    case Ethernet:  return QStringLiteral("Ethernet");
    case Wlan:      return QStringLiteral("Wi-Fi");
    case Bluetooth: return QStringLiteral("Bluetooth");
    case Sonstige:  break;
    }
    return QStringLiteral("Other");
}

Netzquelle::Netzquelle()
{
    // Einmal oeffnen und offen halten. Der Socket kostet nichts und
    // spart je Messung zwei Systemaufrufe.
    m_hci_socket = socket(AfBluetooth, SOCK_RAW, BtprotoHci);
}

Netzquelle::~Netzquelle()
{
    if (m_hci_socket >= 0) {
        close(m_hci_socket);
    }
}

void Netzquelle::rechneRaten(Netzadapter *adapter, qint64 empfangen,
                             qint64 gesendet, qint64 jetzt,
                             QHash<QString, Zaehler> *nachher)
{
    auto vorher = m_vorher.constFind(adapter->name);
    if (vorher != m_vorher.constEnd() && jetzt > vorher->zeitpunkt) {
        const double sekunden = (jetzt - vorher->zeitpunkt) / 1000.0;
        // Die Zaehler koennen zurueckspringen, wenn der Treiber neu
        // geladen wird - bei Bluetooth sind es ausserdem nur 32 Bit,
        // die bei viel Verkehr wirklich ueberlaufen. Negative
        // Differenzen deshalb verwerfen statt anzuzeigen.
        const qint64 d_empfangen = empfangen - vorher->empfangen;
        const qint64 d_gesendet = gesendet - vorher->gesendet;
        if (sekunden > 0 && d_empfangen >= 0 && d_gesendet >= 0) {
            adapter->empfangen = static_cast<qint64>(d_empfangen / sekunden);
            adapter->gesendet = static_cast<qint64>(d_gesendet / sekunden);

            if (adapter->tempo_mbit > 0) {
                // Wie im Original der groessere der beiden Werte, nicht
                // ihre Summe: Senden und Empfangen laufen bei Vollduplex
                // gleichzeitig, die Leitung ist also erst ausgelastet,
                // wenn eine der beiden Richtungen es ist.
                const double bits =
                    8.0 * qMax(adapter->empfangen, adapter->gesendet);
                adapter->auslastung =
                    100.0 * bits / (adapter->tempo_mbit * 1000000.0);
            }
        }
    }
    nachher->insert(adapter->name, Zaehler{empfangen, gesendet, jetzt});
}

void Netzquelle::liesNetz(QVector<Netzadapter> *liste, qint64 jetzt,
                          QHash<QString, Zaehler> *nachher)
{
    QFile datei(QStringLiteral("/proc/net/dev"));
    if (!datei.open(QIODevice::ReadOnly)) {
        return;
    }

    const QList<QByteArray> zeilen = datei.readAll().split('\n');
    for (const QByteArray &zeile : zeilen) {
        // Die ersten beiden Zeilen sind Kopfzeilen; erkennbar daran,
        // dass vor dem Doppelpunkt kein Schnittstellenname steht.
        const int doppelpunkt = zeile.indexOf(':');
        if (doppelpunkt <= 0) {
            continue;
        }
        const QString name =
            QString::fromLatin1(zeile.left(doppelpunkt)).trimmed();
        if (name.isEmpty() || name == QLatin1String("lo")) {
            // Die Rueckschleife bleibt draussen. Sie ist kein Adapter,
            // und ihre Zahlen haetten die Anzeige beherrscht.
            continue;
        }

        const QList<QByteArray> felder =
            zeile.mid(doppelpunkt + 1).simplified().split(' ');
        if (felder.size() < 9) {
            continue;
        }

        Netzadapter a;
        a.name = name;
        a.zustand = sysfsWert(name, QStringLiteral("operstate"));

        // WLAN erkennt man am Verzeichnis wireless/ - das legt nur der
        // WLAN-Teil des Kerns an. Der Name hilft nicht: bei den
        // vorhersagbaren Schnittstellennamen heisst WLAN wlp131s0f0 und
        // Ethernet enp130s0, aber das gilt nur, solange niemand
        // umbenannt hat.
        const bool drahtlos =
            QFile::exists(QStringLiteral("/sys/class/net/") + name
                          + QStringLiteral("/wireless"));
        a.art = drahtlos ? Netzadapter::Wlan : Netzadapter::Ethernet;

        // Bei WLAN steht hier meist nichts oder -1: Die Bitrate wechselt
        // dort mit jedem Paket, und der Treiber gibt sie ueber diesen
        // Weg nicht heraus. Dann bleibt die Auslastung unbekannt - eine
        // erfundene Bezugsgroesse waere schlimmer als eine leere Spalte.
        bool lesbar = false;
        const qint64 tempo =
            sysfsWert(name, QStringLiteral("speed")).toLongLong(&lesbar);
        if (lesbar && tempo > 0) {
            a.tempo_mbit = tempo;
        }

        rechneRaten(&a, felder.at(0).toLongLong(), felder.at(8).toLongLong(),
                    jetzt, nachher);
        liste->append(a);
    }
}

void Netzquelle::liesBluetooth(QVector<Netzadapter> *liste, qint64 jetzt,
                               QHash<QString, Zaehler> *nachher)
{
    if (m_hci_socket < 0) {
        return;
    }

    // Vier Adapter durchprobieren. Wer mehr hat, hat andere Sorgen; die
    // saubere Loesung waere HCIGETDEVLIST, und die kostet eine weitere
    // selbst deklarierte Struktur mit variabler Laenge.
    for (unsigned short kennung = 0; kennung < 4; ++kennung) {
        HciInfo info;
        std::memset(&info, 0, sizeof(info));
        info.dev_id = kennung;
        if (ioctl(m_hci_socket, HciGetDevInfo, &info) < 0) {
            continue;
        }

        Netzadapter a;
        a.name = QString::fromLatin1(info.name, strnlen(info.name, sizeof(info.name)));
        if (a.name.isEmpty()) {
            a.name = QStringLiteral("hci%1").arg(kennung);
        }
        a.art = Netzadapter::Bluetooth;
        a.zustand = (info.flags & HciFlagUp) ? QStringLiteral("up")
                                             : QStringLiteral("down");
        // Bluetooth kennt keine feste Linkgeschwindigkeit, die man
        // abfragen koennte - BR/EDR liegt je nach Paketart zwischen 0,7
        // und 2,1 Mbit/s, BLE noch darunter. Eine Auslastung in Prozent
        // waere hier eine Erfindung, deshalb bleibt sie unbekannt.
        rechneRaten(&a, info.stat.byte_rx, info.stat.byte_tx, jetzt, nachher);
        liste->append(a);
    }
}

QVector<Netzadapter> Netzquelle::lies()
{
    QVector<Netzadapter> liste;
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    QHash<QString, Zaehler> nachher;

    liesNetz(&liste, jetzt, &nachher);
    liesBluetooth(&liste, jetzt, &nachher);

    m_vorher = nachher;
    return liste;
}
