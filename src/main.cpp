// NT Task Manager - ein Systemmonitor in der Form von Windows NT 4.0.
//
// Das Programm setzt weder Stil noch Farben. Es benutzt ausschliesslich
// Qt-Standardwidgets; wie sie aussehen, entscheidet der Widget-Stil des
// Systems. Unter dem Design NT Legacy (das widgetStyle=Windows setzt)
// sieht das Fenster aus wie das Vorbild, unter Breeze wie Breeze. Das
// ist Absicht: ein Programm, das sein Aussehen erzwingt, passt genau
// einmal - und danach nie wieder.
#include "hauptfenster.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication programm(argc, argv);
    QApplication::setApplicationName(QStringLiteral("nt-taskmanager"));
    QApplication::setApplicationDisplayName(QStringLiteral("Task Manager"));
    QApplication::setApplicationVersion(
        QStringLiteral(NT_TASKMANAGER_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("nt-taskmanager"));

    // --bild <datei>: Fenster aufbauen, einmal messen, fotografieren,
    // beenden. Kein Testgeruest im eigentlichen Sinn, sondern der Weg,
    // wie die Bilder fuer das README entstehen - und nebenbei die
    // einzige Moeglichkeit, den Aufbau ohne Sitzung zu pruefen.
    QCommandLineParser zerleger;
    QCommandLineOption bild(QStringLiteral("bild"),
                            QStringLiteral("Fenster in eine Datei schreiben "
                                           "und beenden"),
                            QStringLiteral("datei"));
    QCommandLineOption reiter(QStringLiteral("reiter"),
                              QStringLiteral("Welcher Reiter im Bild zu "
                                             "sehen ist (0 = Processes)"),
                              QStringLiteral("nummer"),
                              QStringLiteral("0"));
    // Nuetzlich fuer den Autostart, und der einzige Weg, den
    // KWin-Umweg fuer "Always On Top" ohne Mausklick auszuloesen.
    QCommandLineOption obenauf(QStringLiteral("obenauf"),
                               QStringLiteral("Fenster immer im Vordergrund "
                                              "halten"));
    zerleger.addOption(bild);
    zerleger.addOption(reiter);
    zerleger.addOption(obenauf);
    zerleger.addHelpOption();
    zerleger.addVersionOption();
    zerleger.process(programm);

    Hauptfenster fenster;
    fenster.zeigeReiter(zerleger.value(reiter).toInt());
    fenster.show();

    if (zerleger.isSet(obenauf)) {
        // Erst nachdem das Fenster steht: KWin findet es sonst noch
        // nicht in seiner Liste.
        QTimer::singleShot(300, &fenster,
                           [&fenster] { fenster.schalteObenauf(true); });
    }

    if (zerleger.isSet(bild)) {
        const QString ziel = zerleger.value(bild);
        // 1,5 Sekunden: der erste Messdurchgang legt nur die
        // Vergleichswerte an, erst der zweite hat eine CPU-Last.
        QTimer::singleShot(1500, &fenster, [&fenster, ziel] {
            fenster.grab().save(ziel);
            fenster.melde();
            QApplication::quit();
        });
    }

    return QApplication::exec();
}
