// Liest die Prozesstabelle aus /proc.
//
// Kein libksysguard, kein procps: was hier gebraucht wird, sind vier
// Dateien je Prozess, und die Formate stehen in proc(5) fest. Die
// Bibliothek waere mehr Abhaengigkeit als Gewinn.
#pragma once

#include <QHash>
#include <QString>
#include <QVector>

struct Prozess {
    int pid = 0;
    QString name;        // comm - der Name, den auch top zeigt
    QString benutzer;    // aufgeloest ueber getpwuid, mit Zwischenspeicher
    double cpu = 0.0;    // Prozent, ueber alle Kerne zusammen wie in NT
    qint64 speicher = 0; // RSS in Kilobyte
    bool eigen = false;  // laeuft unter unserer UID - entscheidet ueber
                         // den Weg beim Beenden
};

class Prozessquelle {
public:
    Prozessquelle();

    // Liest /proc einmal vollstaendig. Die CPU-Last ergibt sich aus dem
    // Abstand zum vorigen Aufruf; der erste Aufruf liefert deshalb
    // ueberall 0,0 - das ist richtig so und keine Luecke.
    QVector<Prozess> lies();

    // Gesamtlast und Speicherbelegung. Alles davon entsteht in lies();
    // hier stehen nur noch Abfragen. Der Reiter "Performance" und die
    // Statuszeile sollen dieselbe Messung sehen und nicht zwei eigene.
    double gesamtlast() const { return m_gesamtlast; }
    double speicherlast() const;
    qint64 speicherBelegt() const { return m_speicher_belegt; }
    qint64 speicherGesamt() const { return m_speicher_gesamt; }

private:
    QString benutzername(uint uid);
    void liesSpeicher();

    struct Zaehler {
        qint64 takte = 0;   // utime + stime beim letzten Lesen
        qint64 zeitpunkt = 0;
    };

    QHash<int, Zaehler> m_vorher;
    QHash<uint, QString> m_namen;   // uid -> Name, getpwuid ist teuer
    qint64 m_gesamt_vorher = 0;     // Summe aller Felder aus /proc/stat
    qint64 m_leerlauf_vorher = 0;
    double m_gesamtlast = 0.0;
    qint64 m_speicher_belegt = 0;   // Kilobyte
    qint64 m_speicher_gesamt = 0;
    long m_takt_je_sekunde = 100;
    int m_kerne = 1;
};
