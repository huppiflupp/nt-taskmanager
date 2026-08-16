#include "mainwindow.h"

#include "networkpage.h"
#include "performancepage.h"
#include "processmodel.h"
#include "servicemodel.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryFile>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <csignal>

namespace {
// One second, as in the original. Shorter is fidgeting, longer swallows
// brief spikes - and those are exactly what one wants to see.
constexpr int TickMs = 1000;

// Name under which the KWin script is registered.
constexpr auto KeepAboveScript = "nt-taskmanager-keepabove";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Task Manager"));

    buildMenu();

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildProcessPage(), QStringLiteral("Processes"));
    m_tabs->addTab(buildServicePage(), QStringLiteral("Services"));
    m_performance = new PerformancePage(this);
    m_tabs->addTab(m_performance, QStringLiteral("Performance"));
    m_network = new NetworkPage(this);
    m_tabs->addTab(m_network, QStringLiteral("Networking"));
    setCentralWidget(m_tabs);

    m_statusProcesses = new QLabel(this);
    m_statusCpu = new QLabel(this);
    m_statusMemory = new QLabel(this);
    for (QLabel *l : {m_statusProcesses, m_statusCpu, m_statusMemory}) {
        statusBar()->addWidget(l, 1);
    }

    m_tick = new QTimer(this);
    m_tick->setInterval(TickMs);
    connect(m_tick, &QTimer::timeout, this, &MainWindow::refresh);
    m_tick->start();

    // Twice right at the start: the first pass only lays down the
    // reference values, the second is the first that can compute a CPU
    // load. Without it everything would read 00 for the first second.
    refresh();
    QTimer::singleShot(200, this, &MainWindow::refresh);

    m_services->refresh();

    resize(424, 468);
}

MainWindow::~MainWindow()
{
    // Remove the "Always On Top" script from KWin again. It would do
    // nothing there any more, but it would sit in the list of loaded
    // scripts until logout - and whoever reads that list should not find
    // corpses in it.
    if (m_alwaysOnTop && m_alwaysOnTop->isChecked()) {
        QDBusInterface(QStringLiteral("org.kde.KWin"),
                       QStringLiteral("/Scripting"),
                       QStringLiteral("org.kde.kwin.Scripting"))
            .call(QStringLiteral("unloadScript"),
                  QLatin1String(KeepAboveScript));
    }
}

void MainWindow::toggleAlwaysOnTop(bool on)
{
    if (m_alwaysOnTop) {
        m_alwaysOnTop->setChecked(on);
    }
}

void MainWindow::showTab(int number)
{
    if (m_tabs && number >= 0 && number < m_tabs->count()) {
        m_tabs->setCurrentIndex(number);
    }
}

void MainWindow::report() const
{
    QTextStream out(stdout);
    out << "Processes: " << m_processes->count()
        << "  CPU: " << qRound(m_processes->totalCpu()) << " %"
        << "  Memory: " << qRound(m_processes->memoryUsage()) << " %"
        << "  Services: " << m_services->count();
    if (!m_services->error().isEmpty()) {
        out << "  ERROR: " << m_services->error();
    }
    out << Qt::endl;
}

void MainWindow::buildMenu()
{
    // The entries of the original, as far as they make sense on Linux.
    // Left out are "Minimize On Use" and "Hide When Minimized" - both
    // are behaviour the window manager governs under Plasma, and no
    // program should take that upon itself. And "Select Columns", as
    // long as there are only five.
    auto *file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QStringLiteral("&New Task (Run...)"), this,
                    &MainWindow::newTask);
    file->addSeparator();
    file->addAction(QStringLiteral("E&xit Task Manager"), this,
                    &QWidget::close);

    auto *options = menuBar()->addMenu(QStringLiteral("&Options"));
    m_alwaysOnTop = options->addAction(QStringLiteral("&Always On Top"));
    m_alwaysOnTop->setCheckable(true);
    connect(m_alwaysOnTop, &QAction::toggled, this, &MainWindow::setAlwaysOnTop);

    auto *view = menuBar()->addMenu(QStringLiteral("&View"));
    view->addAction(QStringLiteral("&Refresh Now"), QKeySequence(Qt::Key_F5),
                    this, &MainWindow::refresh);
    auto *speed = view->addMenu(QStringLiteral("&Update Speed"));
    auto *group = new QActionGroup(this);
    struct Step {
        const char *name;
        int ms;
    };
    // "Paused" is 0 - the timer is then stopped, not set to a very large
    // value. Otherwise a measurement would eventually slip through and
    // the graph would carry a gap nobody can explain.
    for (const Step &s : {Step{"&High", 500}, Step{"&Normal", 1000},
                          Step{"&Low", 4000}, Step{"&Paused", 0}}) {
        auto *entry = speed->addAction(QString::fromLatin1(s.name));
        entry->setCheckable(true);
        entry->setChecked(s.ms == 1000);
        group->addAction(entry);
        connect(entry, &QAction::triggered, this,
                [this, ms = s.ms] { setTickInterval(ms); });
    }

    auto *help = menuBar()->addMenu(QStringLiteral("&Help"));
    help->addAction(QStringLiteral("&About Task Manager"), this,
                    &MainWindow::about);
}

void MainWindow::setAlwaysOnTop(bool on)
{
    // Under X11 there is a window flag for this, and it works.
    if (QGuiApplication::platformName().startsWith(QLatin1String("xcb"))) {
        setWindowFlag(Qt::WindowStaysOnTopHint, on);
        show();
        return;
    }

    // Under Wayland there is not. The protocol has no notion of "always
    // on top" - a window fundamentally cannot decide its own place in
    // the stack there, and Qt::WindowStaysOnTopHint stays without
    // effect. The first attempt set only that flag; that was the reason
    // the menu entry did nothing.
    //
    // The window manager can do it perfectly well, and KWin accepts
    // instructions over D-Bus: a small script that finds our own window
    // by process id and sets keepAbove. Other tools under Plasma take
    // the same route.
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"),
                        QStringLiteral("/Scripting"),
                        QStringLiteral("org.kde.kwin.Scripting"));
    if (!kwin.isValid()) {
        QMessageBox::information(
            this, QStringLiteral("Task Manager"),
            QStringLiteral("\"Always On Top\" needs a window manager that can "
                           "be asked. On Wayland a window cannot raise itself, "
                           "and KWin was not reachable."));
        return;
    }

    QTemporaryFile script(QDir::tempPath()
                          + QStringLiteral("/nt-taskmanager-XXXXXX.js"));
    script.setAutoRemove(false);
    if (!script.open()) {
        return;
    }
    const QString path = script.fileName();
    script.write(QStringLiteral("var windows = workspace.windowList();\n"
                                "for (var i = 0; i < windows.length; i++) {\n"
                                "    if (windows[i].pid == %1) {\n"
                                "        windows[i].keepAbove = %2;\n"
                                "    }\n"
                                "}\n")
                     .arg(QCoreApplication::applicationPid())
                     .arg(on ? QStringLiteral("true") : QStringLiteral("false"))
                     .toUtf8());
    script.close();

    const QString name = QLatin1String(KeepAboveScript);
    // Drop the script of the previous toggle first, otherwise KWin
    // collects one more with every click.
    kwin.call(QStringLiteral("unloadScript"), name);
    const QDBusReply<int> id = kwin.call(QStringLiteral("loadScript"), path, name);
    if (id.isValid()) {
        QDBusInterface(QStringLiteral("org.kde.KWin"),
                       QStringLiteral("/Scripting/Script%1").arg(id.value()),
                       QStringLiteral("org.kde.kwin.Script"))
            .call(QStringLiteral("run"));
    }
    QFile::remove(path);
}

void MainWindow::newTask()
{
    bool confirmed = false;
    const QString command = QInputDialog::getText(
        this, QStringLiteral("Create New Task"),
        QStringLiteral("Type the name of a program, and Task Manager will open "
                       "it for you."),
        QLineEdit::Normal, QString(), &confirmed);
    if (!confirmed || command.trimmed().isEmpty()) {
        return;
    }
    // Through the shell so that "kate file.txt" works too. We trust the
    // input: whoever types here is already sitting in front of the
    // session and could do the same in a terminal.
    if (!QProcess::startDetached(QStringLiteral("/bin/sh"),
                                 {QStringLiteral("-c"), command})) {
        QMessageBox::critical(
            this, QStringLiteral("Task Manager"),
            QStringLiteral("Cannot start \"%1\".").arg(command));
    }
}

void MainWindow::setTickInterval(int milliseconds)
{
    if (milliseconds <= 0) {
        m_tick->stop();
        return;
    }
    m_tick->setInterval(milliseconds);
    m_tick->start();
}

void MainWindow::about()
{
    QMessageBox::about(
        this, QStringLiteral("About Task Manager"),
        QStringLiteral(
            "<b>Task Manager %1</b><br><br>"
            "Processes, services and load in the shape of the Windows NT 4.0 "
            "Task Manager.<br><br>"
            "The window draws nothing by itself - its appearance comes from "
            "the widget style and the colour scheme of the system.<br><br>"
            "GPL-2.0-or-later")
            .arg(QApplication::applicationVersion()));
}

QWidget *MainWindow::buildProcessPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);

    m_processes = new ProcessModel(this);
    m_processFilter = new QSortFilterProxyModel(this);
    m_processFilter->setSourceModel(m_processes);
    m_processFilter->setSortRole(Qt::UserRole);
    m_processFilter->setDynamicSortFilter(true);
    // Without this every capitalised name would stand before every
    // lower-case one - ModemManager before accounts-daemon. By ASCII
    // value that is correct; for a reader it is nonsense.
    m_processFilter->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_processView = new QTableView(page);
    m_processView->setModel(m_processFilter);
    m_processView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_processView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_processView->setSortingEnabled(true);
    m_processView->sortByColumn(ProcessModel::ColumnName, Qt::AscendingOrder);
    m_processView->setShowGrid(false);
    m_processView->setAlternatingRowColors(false);
    m_processView->verticalHeader()->hide();
    m_processView->horizontalHeader()->setStretchLastSection(true);
    m_processView->verticalHeader()->setDefaultSectionSize(
        m_processView->fontMetrics().height() + 4);
    m_processView->setColumnWidth(ProcessModel::ColumnName, 150);
    m_processView->setColumnWidth(ProcessModel::ColumnUser, 90);
    m_processView->setColumnWidth(ProcessModel::ColumnCpu, 40);
    layout->addWidget(m_processView);

    auto *bottom = new QHBoxLayout;
    m_showForeign =
        new QCheckBox(QStringLiteral("Show processes from all users"), page);
    m_showForeign->setChecked(true);
    connect(m_showForeign, &QCheckBox::toggled, this, &MainWindow::refresh);
    bottom->addWidget(m_showForeign);
    bottom->addStretch();

    auto *endButton = new QPushButton(QStringLiteral("End Process"), page);
    connect(endButton, &QPushButton::clicked, this,
            &MainWindow::endSelectedProcess);
    bottom->addWidget(endButton);
    layout->addLayout(bottom);

    return page;
}

QWidget *MainWindow::buildServicePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);

    m_services = new ServiceModel(this);
    m_serviceFilter = new QSortFilterProxyModel(this);
    m_serviceFilter->setSourceModel(m_services);
    m_serviceFilter->setSortRole(Qt::UserRole);
    m_serviceFilter->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_serviceView = new QTableView(page);
    m_serviceView->setModel(m_serviceFilter);
    m_serviceView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_serviceView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_serviceView->setSortingEnabled(true);
    m_serviceView->sortByColumn(ServiceModel::ColumnName, Qt::AscendingOrder);
    m_serviceView->setShowGrid(false);
    m_serviceView->verticalHeader()->hide();
    m_serviceView->horizontalHeader()->setStretchLastSection(true);
    m_serviceView->verticalHeader()->setDefaultSectionSize(
        m_serviceView->fontMetrics().height() + 4);
    m_serviceView->setColumnWidth(ServiceModel::ColumnName, 170);
    m_serviceView->setColumnWidth(ServiceModel::ColumnDescription, 150);
    layout->addWidget(m_serviceView);

    return page;
}

void MainWindow::rememberSelection()
{
    m_rememberedPid = -1;
    const QModelIndexList selected =
        m_processView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    const QModelIndex source = m_processFilter->mapToSource(selected.first());
    if (const Process *p = m_processes->process(source.row())) {
        m_rememberedPid = p->pid;
    }
}

void MainWindow::restoreSelection()
{
    if (m_rememberedPid < 0) {
        return;
    }
    for (int row = 0; row < m_processes->count(); ++row) {
        const Process *p = m_processes->process(row);
        if (!p || p->pid != m_rememberedPid) {
            continue;
        }
        const QModelIndex target =
            m_processFilter->mapFromSource(m_processes->index(row, 0));
        if (target.isValid()) {
            m_processView->selectionModel()->select(
                target,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_processView->setCurrentIndex(target);
        }
        return;
    }
}

void MainWindow::refresh()
{
    // The selection does not survive a model reset. So remember the pid
    // beforehand and look it up again afterwards - the user should not
    // lose the row while aiming at "End Process" just because some
    // process started somewhere.
    rememberSelection();
    m_processes->refresh(m_showForeign && m_showForeign->isChecked());
    restoreSelection();

    m_statusProcesses->setText(
        QStringLiteral("Processes: %1").arg(m_processes->count()));
    m_statusCpu->setText(
        QStringLiteral("CPU Usage: %1%").arg(qRound(m_processes->totalCpu())));
    m_statusMemory->setText(QStringLiteral("Physical Memory: %1%")
                                .arg(qRound(m_processes->memoryUsage())));

    if (m_network) {
        m_network->refresh();
    }

    // Pass the same numbers on rather than measuring them again there -
    // otherwise the same window would show two values for the same
    // quantity, differing by a per cent.
    if (m_performance) {
        m_performance->setValues(
            m_processes->totalCpu(), m_processes->memoryUsage(),
            m_processes->memoryUsed(), m_processes->memoryTotal());
    }
}

void MainWindow::endSelectedProcess()
{
    rememberSelection();
    if (m_rememberedPid < 0) {
        return;
    }

    int row = -1;
    for (int i = 0; i < m_processes->count(); ++i) {
        if (m_processes->process(i)->pid == m_rememberedPid) {
            row = i;
            break;
        }
    }
    if (row < 0) {
        return;
    }
    const Process p = *m_processes->process(row);

    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Task Manager"),
        QStringLiteral("Terminating a process can cause undesired results "
                       "including loss of data and system instability.\n\n"
                       "Are you sure you want to terminate \"%1\" (PID %2)?")
            .arg(p.name)
            .arg(p.pid),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    // SIGTERM, not SIGKILL: the process should be allowed to clean up.
    // The Windows task manager is blunter here, but on Linux an unclosed
    // filesystem journal is a price nobody wants to pay for one click.
    if (p.own) {
        if (::kill(p.pid, SIGTERM) == 0) {
            refresh();
            return;
        }
    }

    // Foreign process, or no permission: through pkexec, which opens the
    // authentication dialog of the system. No Polkit code of our own -
    // that is what the tool is for.
    QProcess elevated;
    elevated.start(QStringLiteral("pkexec"),
                   {QStringLiteral("kill"), QStringLiteral("-TERM"),
                    QString::number(p.pid)});
    if (!elevated.waitForFinished(30000) || elevated.exitCode() != 0) {
        QMessageBox::critical(
            this, QStringLiteral("Task Manager"),
            QStringLiteral("The process \"%1\" could not be terminated.")
                .arg(p.name));
    }
    refresh();
}
