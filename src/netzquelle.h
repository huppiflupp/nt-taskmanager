// Die Netzwerkschnittstellen und ihre Auslastung.
//
// Drei Quellen, weil drei Arten von Adapter gezeigt werden:
//
//   Ethernet, WLAN  /proc/net/dev fuer die Zaehler, /sys/class/net fuer
//                   Zustand und Linkgeschwindigkeit
//   Bluetooth       ioctl HCIGETDEVINFO auf einem HCI-Socket - derselbe
//                   Weg, den hciconfig geht, und ohne besondere Rechte
//
// Bluetooth taucht in /proc/net/dev nicht auf. Dort stuende nur bnep0,
// und auch das nur, solange gerade eine PAN-Verbindung besteht - der
// Adapter selbst und sein Verkehr bleiben unsichtbar.
#pragma once

#include <QHash>
#include <QString>
#include <QVector>

struct Netzadapter {
    enum Art { Ethernet, Wlan, Bluetooth, Sonstige };

    QString name;
    Art art = Sonstige;
    QString zustand;        // up, down, unknown
    qint64 tempo_mbit = -1; // -1: die Schnittstelle sagt es nicht
    double auslastung = -1; // Prozent der Linkgeschwindigkeit, -1 unbekannt
    qint64 empfangen = 0;   // Byte je Sekunde
    qint64 gesendet = 0;

    QString artname() const;
};

class Netzquelle {
public:
    Netzquelle();
    ~Netzquelle();

    Netzquelle(const Netzquelle &) = delete;
    Netzquelle &operator=(const Netzquelle &) = delete;

    // Einmal messen. Wie bei den Prozessen ergeben sich die Raten aus
    // dem Abstand zum vorigen Aufruf - der erste liefert ueberall 0.
    QVector<Netzadapter> lies();

private:
    struct Zaehler {
        qint64 empfangen = 0;
        qint64 gesendet = 0;
        qint64 zeitpunkt = 0;
    };

    void liesNetz(QVector<Netzadapter> *liste, qint64 jetzt,
                  QHash<QString, Zaehler> *nachher);
    void liesBluetooth(QVector<Netzadapter> *liste, qint64 jetzt,
                       QHash<QString, Zaehler> *nachher);
    void rechneRaten(Netzadapter *adapter, qint64 empfangen, qint64 gesendet,
                     qint64 jetzt, QHash<QString, Zaehler> *nachher);

    QHash<QString, Zaehler> m_vorher;
    int m_hci_socket = -1;
};
