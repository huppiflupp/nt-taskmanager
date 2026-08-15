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

int main(int argc, char *argv[])
{
    QApplication programm(argc, argv);
    QApplication::setApplicationName(QStringLiteral("nt-taskmanager"));
    QApplication::setApplicationDisplayName(QStringLiteral("Task Manager"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setDesktopFileName(QStringLiteral("nt-taskmanager"));

    Hauptfenster fenster;
    fenster.show();

    return QApplication::exec();
}
