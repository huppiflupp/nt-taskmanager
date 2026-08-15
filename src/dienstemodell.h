// Die systemd-Units als Qt-Modell.
//
// Der Windows-Taskmanager zeigt unter "Services" die Dienste des
// Systems. Die Linux-Entsprechung sind systemd-Units, und die holt man
// nicht ueber ein Kommandozeilenwerkzeug ab: systemctl selbst spricht
// D-Bus, wir tun dasselbe direkt. Das spart das Zerlegen von Ausgaben,
// die sich mit jeder Version aendern koennen.
#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct Dienst {
    QString name;         // sshd.service
    QString beschreibung; // "OpenSSH server daemon"
    QString geladen;      // loaded, not-found, masked
    QString aktiv;        // active, inactive, failed
    QString zustand;      // running, exited, dead
    QString pfad;         // Objektpfad, fuer Start und Stopp
};

class Dienstemodell : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Spalte {
        SpalteName = 0,
        SpalteBeschreibung,
        SpalteAktiv,
        SpalteZustand,
        SpaltenZahl
    };

    explicit Dienstemodell(QObject *eltern = nullptr);

    int rowCount(const QModelIndex &eltern = {}) const override;
    int columnCount(const QModelIndex &eltern = {}) const override;
    QVariant data(const QModelIndex &index, int rolle) const override;
    QVariant headerData(int abschnitt, Qt::Orientation richtung,
                        int rolle) const override;

    void aktualisiere();
    const Dienst *dienst(int zeile) const;
    int anzahl() const { return static_cast<int>(m_zeilen.size()); }

    // Gibt eine Fehlermeldung zurueck, wenn der Systembus nicht
    // erreichbar ist - dann steht sie in der Statuszeile, statt dass
    // eine leere Tabelle unerklaert bleibt.
    QString fehler() const { return m_fehler; }

private:
    QVector<Dienst> m_zeilen;
    QString m_fehler;
};
