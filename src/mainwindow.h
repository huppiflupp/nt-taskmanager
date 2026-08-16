// The window.
//
// It draws nothing by itself and sets no colours. Everything here that
// looks like Windows NT comes from the widget style in use and from the
// colour scheme - see the README. A stylesheet or a palette of its own
// would be exactly the mistake this program avoids: it would then look
// wrong under every other theme.
#pragma once

#include <QMainWindow>

class QAction;
class QCheckBox;
class QLabel;
class QSortFilterProxyModel;
class QTabWidget;
class QTableView;
class QTimer;

class NetworkPage;
class PerformancePage;
class ProcessModel;
class ServiceModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Writes the key figures to standard output. For --image, where
    // nobody is watching but someone should be able to check the sums.
    void report() const;
    void showTab(int number);

    // For the --on-top start option. Goes through the menu entry so the
    // check mark and reality do not drift apart.
    void toggleAlwaysOnTop(bool on);

private Q_SLOTS:
    void refresh();
    void endSelectedProcess();
    void newTask();
    void setAlwaysOnTop(bool on);
    void setTickInterval(int milliseconds);
    void about();

private:
    void buildMenu();
    QWidget *buildProcessPage();
    QWidget *buildServicePage();
    void rememberSelection();
    void restoreSelection();

    ProcessModel *m_processes = nullptr;
    QSortFilterProxyModel *m_processFilter = nullptr;
    QTableView *m_processView = nullptr;
    QCheckBox *m_showForeign = nullptr;

    ServiceModel *m_services = nullptr;
    QSortFilterProxyModel *m_serviceFilter = nullptr;
    QTableView *m_serviceView = nullptr;

    QLabel *m_statusProcesses = nullptr;
    QLabel *m_statusCpu = nullptr;
    QLabel *m_statusMemory = nullptr;

    QAction *m_alwaysOnTop = nullptr;
    PerformancePage *m_performance = nullptr;
    NetworkPage *m_network = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTimer *m_tick = nullptr;
    int m_rememberedPid = -1;
};
