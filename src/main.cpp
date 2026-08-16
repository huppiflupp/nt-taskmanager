// NT Task Manager - a system monitor in the shape of Windows NT 4.0.
//
// The program sets neither style nor colours. It uses nothing but
// standard Qt widgets; how they look is decided by the widget style of
// the system. Under the NT Legacy theme (which sets widgetStyle=Windows)
// the window looks like its model, under Breeze it looks like Breeze.
// That is intended: a program that forces its appearance fits exactly
// once - and never again after that.
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("nt-taskmanager"));
    QApplication::setApplicationDisplayName(QStringLiteral("Task Manager"));
    QApplication::setApplicationVersion(QStringLiteral(NT_TASKMANAGER_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("nt-taskmanager"));

    QCommandLineParser parser;
    QCommandLineOption image(
        QStringLiteral("image"),
        QStringLiteral("Write the window to a file and quit"),
        QStringLiteral("file"));
    QCommandLineOption tab(
        QStringLiteral("tab"),
        QStringLiteral("Which tab to open with, 0 being Processes"),
        QStringLiteral("number"), QStringLiteral("0"));
    // Useful for autostart, and the only way to trigger the KWin detour
    // for "Always On Top" without a mouse click.
    QCommandLineOption onTop(QStringLiteral("on-top"),
                             QStringLiteral("Keep the window above others"));
    parser.addOption(image);
    parser.addOption(tab);
    parser.addOption(onTop);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    MainWindow window;
    window.showTab(parser.value(tab).toInt());
    window.show();

    if (parser.isSet(onTop)) {
        // Only once the window stands: KWin would not find it in its
        // list before that.
        QTimer::singleShot(300, &window,
                           [&window] { window.toggleAlwaysOnTop(true); });
    }

    // --image: build the window, measure once, take the picture, quit.
    // Not a test harness as such, but the way the pictures for the
    // README are made - and incidentally the only way to check the
    // layout without a session.
    if (parser.isSet(image)) {
        const QString target = parser.value(image);
        // 1.5 seconds: the first pass only lays down the reference
        // values, the second is the first with a CPU load.
        QTimer::singleShot(1500, &window, [&window, target] {
            window.grab().save(target);
            window.report();
            QApplication::quit();
        });
    }

    return QApplication::exec();
}
