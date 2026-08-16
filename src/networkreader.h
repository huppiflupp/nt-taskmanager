// The network interfaces and their utilisation.
//
// Three sources, because three kinds of adapter are shown:
//
//   Ethernet, Wi-Fi  /proc/net/dev for the counters, /sys/class/net for
//                    state and link speed
//   Bluetooth        ioctl HCIGETDEVINFO on an HCI socket - the same way
//                    hciconfig goes, and without special privileges
//
// Bluetooth does not appear in /proc/net/dev. At most bnep0 would show
// there, and only while a PAN connection exists - the adapter itself and
// its traffic stay invisible.
#pragma once

#include <QHash>
#include <QString>
#include <QVector>

struct NetworkAdapter {
    enum Kind { Ethernet, WiFi, Bluetooth, Other };

    QString name;
    Kind kind = Other;
    QString state;          // up, down, unknown
    qint64 speedMbit = -1;  // -1: the interface does not say
    double utilisation = -1; // per cent of link speed, -1 unknown
    qint64 received = 0;    // bytes per second
    qint64 sent = 0;

    QString kindName() const;
};

class NetworkReader {
public:
    NetworkReader();
    ~NetworkReader();

    NetworkReader(const NetworkReader &) = delete;
    NetworkReader &operator=(const NetworkReader &) = delete;

    // Take one measurement. As with the processes, the rates follow from
    // the distance to the previous call - the first one returns 0.
    QVector<NetworkAdapter> read();

private:
    struct Counter {
        qint64 received = 0;
        qint64 sent = 0;
        qint64 stamp = 0;
    };

    void readInterfaces(QVector<NetworkAdapter> *list, qint64 now,
                        QHash<QString, Counter> *current);
    void readBluetooth(QVector<NetworkAdapter> *list, qint64 now,
                       QHash<QString, Counter> *current);
    void computeRates(NetworkAdapter *adapter, qint64 received, qint64 sent,
                      qint64 now, QHash<QString, Counter> *current);

    QHash<QString, Counter> m_previous;
    int m_hciSocket = -1;
};
