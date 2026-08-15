#include "prozessquelle.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <pwd.h>
#include <unistd.h>

Prozessquelle::Prozessquelle()
{
    m_takt_je_sekunde = sysconf(_SC_CLK_TCK);
    if (m_takt_je_sekunde <= 0) {
        m_takt_je_sekunde = 100;
    }
    m_kerne = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (m_kerne <= 0) {
        m_kerne = 1;
    }
}

QString Prozessquelle::benutzername(uint uid)
{
    auto treffer = m_namen.constFind(uid);
    if (treffer != m_namen.constEnd()) {
        return *treffer;
    }
    // getpwuid geht bei Netzwerkverzeichnissen ueber das Netz. Bei 500
    // Prozessen je Aktualisierung waere das die teuerste Stelle im
    // ganzen Programm - deshalb der Zwischenspeicher.
    QString name = QString::number(uid);
    if (const passwd *eintrag = getpwuid(uid)) {
        name = QString::fromLocal8Bit(eintrag->pw_name);
    }
    m_namen.insert(uid, name);
    return name;
}

// Der Name in /proc/<pid>/stat steht in Klammern und darf selbst
// Klammern und Leerzeichen enthalten ("(Web Content)"). Deshalb wird
// hinter der LETZTEN schliessenden Klammer weitergelesen und nicht
// stumpf an Leerzeichen geteilt - ein Fehler, der erst auffaellt, wenn
// jemand einen Prozess mit Leerzeichen im Namen startet.
static bool takte_aus_stat(const QByteArray &inhalt, qint64 *takte)
{
    const int zu = inhalt.lastIndexOf(')');
    if (zu < 0) {
        return false;
    }
    const QList<QByteArray> felder = inhalt.mid(zu + 2).split(' ');
    // Ab hier ist Feld 0 = state (Feld 3 in proc(5)). utime ist Feld 14,
    // stime Feld 15 - also Index 11 und 12 in dieser Zaehlung.
    if (felder.size() < 13) {
        return false;
    }
    *takte = felder.at(11).toLongLong() + felder.at(12).toLongLong();
    return true;
}

QVector<Prozess> Prozessquelle::lies()
{
    QVector<Prozess> liste;
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    const uint eigene_uid = getuid();

    QHash<int, Zaehler> nachher;
    QDir proc(QStringLiteral("/proc"));
    const QStringList eintraege =
        proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);

    for (const QString &eintrag : eintraege) {
        bool ist_zahl = false;
        const int pid = eintrag.toInt(&ist_zahl);
        if (!ist_zahl) {
            continue;
        }

        const QString basis = QStringLiteral("/proc/") + eintrag;

        // Prozesse verschwinden waehrend des Lesens. Jede fehlgeschlagene
        // Datei ist deshalb ein normaler Fall, kein Fehler.
        QFile stat(basis + QStringLiteral("/stat"));
        if (!stat.open(QIODevice::ReadOnly)) {
            continue;
        }
        qint64 takte = 0;
        if (!takte_aus_stat(stat.readAll(), &takte)) {
            continue;
        }

        Prozess p;
        p.pid = pid;

        // Der Name kommt aus dem Programmpfad, nicht aus comm.
        //
        // comm ist auf 15 Zeichen begrenzt - eine Grenze aus dem Kern,
        // die es seit jeher gibt. Aus plasma-systemmonitor wird dort
        // "plasma-systemmo", und die Namensspalte ist die wichtigste
        // Spalte dieses Programms. Der Symlink exe zeigt auf die
        // wirkliche Datei, ohne Laengengrenze.
        //
        // Kein Zwischenspeicher dafuer: readlink ist ein Systemaufruf
        // ohne Zugriff auf den Datentraeger, 500 Stueck je Sekunde
        // fallen nicht auf. Ein Zwischenspeicher haette dagegen ein
        // echtes Problem - PIDs werden wiederverwendet, und ein neuer
        // Prozess bekaeme den Namen seines Vorgaengers.
        const QString programm = QFileInfo(basis + QStringLiteral("/exe"))
                                     .symLinkTarget();
        if (!programm.isEmpty()) {
            p.name = QFileInfo(programm).fileName();
        }
        if (p.name.isEmpty()) {
            // Kein exe zu sehen. Das hat zwei ganz verschiedene Gruende,
            // und sie auseinanderzuhalten ist noetig: Kernprozesse haben
            // wirklich keines, fremde Prozesse geben es nur nicht preis.
            // cmdline unterscheidet beide - die haben nur Kernprozesse
            // nicht. (Erster Versuch nahm dafuer, ob exe existiert.
            // Das lieferte "[ModemManager]", weil exists() dem Symlink
            // folgt und ohne Leserecht ebenfalls nein sagt.)
            QFile cmdline(basis + QStringLiteral("/cmdline"));
            QByteArray befehl;
            if (cmdline.open(QIODevice::ReadOnly)) {
                befehl = cmdline.readAll();
            }
            if (!befehl.isEmpty()) {
                const QString erstes =
                    QString::fromLocal8Bit(befehl.split('\0').constFirst());
                p.name = QFileInfo(erstes).fileName();
            }
            if (p.name.isEmpty()) {
                QFile comm(basis + QStringLiteral("/comm"));
                if (comm.open(QIODevice::ReadOnly)) {
                    p.name = QString::fromLocal8Bit(comm.readAll()).trimmed();
                }
                if (p.name.isEmpty()) {
                    continue;
                }
                // Eckige Klammern fuer Kernprozesse, wie ps sie schreibt.
                // Nicht fuer die, die schon eigene Klammern tragen -
                // "(sd-pam)" soll nicht "[(sd-pam)]" heissen.
                if (befehl.isEmpty() && !p.name.startsWith(QLatin1Char('('))) {
                    p.name = QLatin1Char('[') + p.name + QLatin1Char(']');
                }
            }
        }

        QFile statm(basis + QStringLiteral("/statm"));
        if (statm.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> f = statm.readAll().split(' ');
            if (f.size() > 1) {
                // Feld 2 ist resident, gezaehlt in Seiten.
                p.speicher = f.at(1).toLongLong() * (sysconf(_SC_PAGESIZE) / 1024);
            }
        }

        const uint uid = QFileInfo(basis).ownerId();
        p.benutzer = benutzername(uid);
        p.eigen = (uid == eigene_uid);

        // Last aus dem Abstand zum vorigen Lesen. Bezugsgroesse ist die
        // gesamte verfuegbare Rechenzeit aller Kerne - so summieren sich
        // die Werte auf 100 Prozent, wie es der Windows-Taskmanager
        // zeigt, und nicht auf 100 je Kern wie bei top.
        auto vorher = m_vorher.constFind(pid);
        if (vorher != m_vorher.constEnd() && jetzt > vorher->zeitpunkt) {
            const qint64 delta_takte = takte - vorher->takte;
            const double sekunden = (jetzt - vorher->zeitpunkt) / 1000.0;
            if (delta_takte > 0 && sekunden > 0) {
                p.cpu = 100.0 * delta_takte
                        / (sekunden * m_takt_je_sekunde * m_kerne);
            }
        }
        nachher.insert(pid, Zaehler{takte, jetzt});

        liste.append(p);
    }

    m_vorher = nachher;

    // Gesamtlast aus /proc/stat, nicht als Summe der Einzelwerte: die
    // verpasst alles, was zwischen zwei Messungen entstanden und wieder
    // verschwunden ist.
    QFile gesamt(QStringLiteral("/proc/stat"));
    if (gesamt.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> f =
            gesamt.readLine().simplified().split(' ');
        if (f.size() > 4) {
            qint64 summe = 0;
            for (int i = 1; i < f.size(); ++i) {
                summe += f.at(i).toLongLong();
            }
            const qint64 leerlauf = f.at(4).toLongLong();
            const qint64 d_summe = summe - m_gesamt_vorher;
            const qint64 d_leerlauf = leerlauf - m_leerlauf_vorher;
            if (m_gesamt_vorher > 0 && d_summe > 0) {
                m_gesamtlast = 100.0 * (d_summe - d_leerlauf) / d_summe;
            }
            m_gesamt_vorher = summe;
            m_leerlauf_vorher = leerlauf;
        }
    }

    return liste;
}

double Prozessquelle::speicherlast() const
{
    QFile datei(QStringLiteral("/proc/meminfo"));
    if (!datei.open(QIODevice::ReadOnly)) {
        return 0.0;
    }
    qint64 gesamt = 0;
    qint64 verfuegbar = 0;
    // readAll und dann teilen, NICHT while (!atEnd()) readLine().
    //
    // Dateien unter /proc melden die Groesse 0 - sie entstehen erst beim
    // Lesen. QFile::atEnd() beantwortet die Frage aber anhand genau
    // dieser Groesse und sagt schon vor dem ersten Lesen "ja". Die
    // Schleife lief deshalb null Mal durch, die Speicheranzeige stand
    // auf 0 Prozent, und es sah aus wie ein Rechenfehler. readAll()
    // liest blockweise weiter, bis nichts mehr kommt, und liefert die
    // vollen 1671 Bytes.
    const QList<QByteArray> zeilen = datei.readAll().split('\n');
    for (const QByteArray &zeile : zeilen) {
        // MemAvailable, nicht MemFree: was der Zwischenspeicher haelt,
        // ist nicht belegt, sondern jederzeit abrufbar. MemFree zeigte
        // auf einem gesunden System 95 Prozent Belegung an.
        if (zeile.startsWith("MemTotal:")) {
            gesamt = zeile.split(':').at(1).simplified().split(' ').at(0).toLongLong();
        } else if (zeile.startsWith("MemAvailable:")) {
            verfuegbar = zeile.split(':').at(1).simplified().split(' ').at(0).toLongLong();
            break;
        }
    }
    if (gesamt <= 0) {
        return 0.0;
    }
    return 100.0 * (gesamt - verfuegbar) / gesamt;
}
