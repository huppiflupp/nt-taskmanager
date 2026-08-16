#include "networkreader.h"

#include <QDateTime>
#include <QFile>

#include <cstring>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

QString sysfsValue(const QString &interface, const QString &file)
{
    QFile f(QStringLiteral("/sys/class/net/") + interface + QLatin1Char('/')
            + file);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    // readAll, not readLine with atEnd: sysfs reports a size of 0 just
    // like procfs.
    return QString::fromLatin1(f.readAll()).trimmed();
}

// ── Bluetooth ────────────────────────────────────────────────────────
//
// The values come from the kernel by ioctl. The structs live in
// <bluetooth/hci.h>, and that header belongs to bluez-libs-devel - a
// build dependency for two numbers would be too much to ask. So they are
// declared here. It is a kernel ABI and therefore stable; verified
// against hciconfig on this machine: RX 2988490 and TX 30409, equal to
// the byte.
constexpr int AfBluetooth = 31;
constexpr int BtprotoHci = 1;
// Not the precomputed number (0x800448d3) but the macro: it assembles
// the value from direction, size and number, which also holds on
// architectures that arrange those bits differently.
constexpr unsigned long HciGetDevInfo = _IOR('H', 211, int);

struct HciStats {
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
    HciStats stat;
};

constexpr unsigned int HciFlagUp = 1U << 0;

} // namespace

QString NetworkAdapter::kindName() const
{
    switch (kind) {
    case Ethernet:  return QStringLiteral("Ethernet");
    case WiFi:      return QStringLiteral("Wi-Fi");
    case Bluetooth: return QStringLiteral("Bluetooth");
    case Other:     break;
    }
    return QStringLiteral("Other");
}

NetworkReader::NetworkReader()
{
    // Open once and keep it. The socket costs nothing and saves two
    // syscalls per measurement.
    m_hciSocket = socket(AfBluetooth, SOCK_RAW, BtprotoHci);
}

NetworkReader::~NetworkReader()
{
    if (m_hciSocket >= 0) {
        close(m_hciSocket);
    }
}

void NetworkReader::computeRates(NetworkAdapter *adapter, qint64 received,
                                 qint64 sent, qint64 now,
                                 QHash<QString, Counter> *current)
{
    auto previous = m_previous.constFind(adapter->name);
    if (previous != m_previous.constEnd() && now > previous->stamp) {
        const double seconds = (now - previous->stamp) / 1000.0;
        // Counters can jump backwards when a driver is reloaded - for
        // Bluetooth they are only 32 bits wide and do wrap under heavy
        // traffic. Negative differences are therefore discarded rather
        // than displayed.
        const qint64 deltaReceived = received - previous->received;
        const qint64 deltaSent = sent - previous->sent;
        if (seconds > 0 && deltaReceived >= 0 && deltaSent >= 0) {
            adapter->received = static_cast<qint64>(deltaReceived / seconds);
            adapter->sent = static_cast<qint64>(deltaSent / seconds);

            if (adapter->speedMbit > 0) {
                // As in the original the larger of the two, not their
                // sum: sending and receiving run at the same time on a
                // full duplex link, so the line is only busy when one of
                // the directions is.
                const double bits = 8.0 * qMax(adapter->received, adapter->sent);
                adapter->utilisation =
                    100.0 * bits / (adapter->speedMbit * 1000000.0);
            }
        }
    }
    current->insert(adapter->name, Counter{received, sent, now});
}

void NetworkReader::readInterfaces(QVector<NetworkAdapter> *list, qint64 now,
                                   QHash<QString, Counter> *current)
{
    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QList<QByteArray> lines = file.readAll().split('\n');
    for (const QByteArray &line : lines) {
        // The first two lines are headers; recognisable because no
        // interface name stands before the colon.
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QString name = QString::fromLatin1(line.left(colon)).trimmed();
        if (name.isEmpty() || name == QLatin1String("lo")) {
            // Loopback stays out. It is not an adapter, and its numbers
            // would have dominated the display.
            continue;
        }

        const QList<QByteArray> fields =
            line.mid(colon + 1).simplified().split(' ');
        if (fields.size() < 9) {
            continue;
        }

        NetworkAdapter a;
        a.name = name;
        a.state = sysfsValue(name, QStringLiteral("operstate"));

        // Wi-Fi is recognised by the wireless/ directory - only the
        // wireless part of the kernel creates it. The name is no help:
        // with predictable interface names Wi-Fi is wlp131s0f0 and
        // Ethernet enp130s0, but only until someone renames them.
        const bool wireless = QFile::exists(QStringLiteral("/sys/class/net/")
                                            + name + QStringLiteral("/wireless"));
        a.kind = wireless ? NetworkAdapter::WiFi : NetworkAdapter::Ethernet;

        // For Wi-Fi this is usually empty or -1: the bit rate changes
        // with every packet there, and the driver does not expose it
        // this way. Then utilisation stays unknown - an invented
        // reference would be worse than an empty column.
        bool readable = false;
        const qint64 speed =
            sysfsValue(name, QStringLiteral("speed")).toLongLong(&readable);
        if (readable && speed > 0) {
            a.speedMbit = speed;
        }

        computeRates(&a, fields.at(0).toLongLong(), fields.at(8).toLongLong(),
                     now, current);
        list->append(a);
    }
}

void NetworkReader::readBluetooth(QVector<NetworkAdapter> *list, qint64 now,
                                  QHash<QString, Counter> *current)
{
    if (m_hciSocket < 0) {
        return;
    }

    // Try four adapters. Anyone with more has other problems; the clean
    // solution would be HCIGETDEVLIST, and that costs another
    // hand-declared struct of variable length.
    for (unsigned short id = 0; id < 4; ++id) {
        HciInfo info;
        std::memset(&info, 0, sizeof(info));
        info.dev_id = id;
        if (ioctl(m_hciSocket, HciGetDevInfo, &info) < 0) {
            continue;
        }

        NetworkAdapter a;
        a.name = QString::fromLatin1(info.name,
                                     strnlen(info.name, sizeof(info.name)));
        if (a.name.isEmpty()) {
            a.name = QStringLiteral("hci%1").arg(id);
        }
        a.kind = NetworkAdapter::Bluetooth;
        a.state = (info.flags & HciFlagUp) ? QStringLiteral("up")
                                           : QStringLiteral("down");
        // Bluetooth has no fixed link speed to query - BR/EDR sits
        // between 0.7 and 2.1 Mbit/s depending on packet type, BLE below
        // that. A utilisation in per cent would be an invention here, so
        // it stays unknown.
        computeRates(&a, info.stat.byte_rx, info.stat.byte_tx, now, current);
        list->append(a);
    }
}

QVector<NetworkAdapter> NetworkReader::read()
{
    QVector<NetworkAdapter> list;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QHash<QString, Counter> current;

    readInterfaces(&list, now, &current);
    readBluetooth(&list, now, &current);

    m_previous = current;
    return list;
}
