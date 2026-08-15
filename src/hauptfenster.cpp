#include "hauptfenster.h"

#include "dienstemodell.h"
#include "leistungsseite.h"
#include "prozessmodell.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextStream>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <csignal>

namespace {
// Eine Sekunde, wie im Original. Kuerzer waere Zappeln, laenger
// verschluckt kurze Lastspitzen - und die will man ja gerade sehen.
constexpr int TaktMs = 1000;
}

Hauptfenster::Hauptfenster(QWidget *eltern)
    : QMainWindow(eltern)
{
    setWindowTitle(QStringLiteral("Task Manager"));

    baueMenue();

    m_reiter = new QTabWidget(this);
    m_reiter->addTab(baueProzessseite(), QStringLiteral("Processes"));
    m_reiter->addTab(baueDiensteseite(), QStringLiteral("Services"));
    m_leistung = new Leistungsseite(this);
    m_reiter->addTab(m_leistung, QStringLiteral("Performance"));
    setCentralWidget(m_reiter);

    m_standProzesse = new QLabel(this);
    m_standCpu = new QLabel(this);
    m_standSpeicher = new QLabel(this);
    for (QLabel *l : {m_standProzesse, m_standCpu, m_standSpeicher}) {
        statusBar()->addWidget(l, 1);
    }

    m_takt = new QTimer(this);
    m_takt->setInterval(TaktMs);
    connect(m_takt, &QTimer::timeout, this, &Hauptfenster::aktualisiere);
    m_takt->start();

    // Zweimal gleich zu Beginn: der erste Durchgang legt nur die
    // Vergleichswerte an, erst der zweite kann eine CPU-Last ausrechnen.
    // Ohne das stuende in der ersten Sekunde ueberall 00.
    aktualisiere();
    QTimer::singleShot(200, this, &Hauptfenster::aktualisiere);

    m_dienste->aktualisiere();

    resize(424, 468);
}

void Hauptfenster::zeigeReiter(int nummer)
{
    if (m_reiter && nummer >= 0 && nummer < m_reiter->count()) {
        m_reiter->setCurrentIndex(nummer);
    }
}

void Hauptfenster::melde() const
{
    QTextStream aus(stdout);
    aus << "Prozesse: " << m_prozesse->anzahl()
        << "  CPU: " << qRound(m_prozesse->gesamtlast()) << " %"
        << "  Speicher: " << qRound(m_prozesse->speicherlast()) << " %"
        << "  Dienste: " << m_dienste->anzahl();
    if (!m_dienste->fehler().isEmpty()) {
        aus << "  FEHLER: " << m_dienste->fehler();
    }
    aus << Qt::endl;
}

void Hauptfenster::baueMenue()
{
    // Die Eintraege des Originals, soweit sie unter Linux einen Sinn
    // ergeben. Weggelassen sind "Minimize On Use" und "Hide When
    // Minimized" - beides Verhalten, das unter Plasma der
    // Fenstermanager regelt und kein Programm sich selbst nehmen
    // sollte. Und "Select Columns", solange es nur fuenf gibt.
    auto *datei = menuBar()->addMenu(QStringLiteral("&File"));
    datei->addAction(QStringLiteral("&New Task (Run...)"), this,
                     &Hauptfenster::neueAufgabe);
    datei->addSeparator();
    datei->addAction(QStringLiteral("E&xit Task Manager"), this,
                     &QWidget::close);

    auto *einstellungen = menuBar()->addMenu(QStringLiteral("&Options"));
    auto *obenauf = einstellungen->addAction(QStringLiteral("&Always On Top"));
    obenauf->setCheckable(true);
    connect(obenauf, &QAction::toggled, this, [this](bool an) {
        // Fensterfahnen zu aendern heisst unter Wayland, das Fenster neu
        // aufzubauen - deshalb das show() danach. Ohne es verschwindet
        // das Fenster beim Umschalten.
        setWindowFlag(Qt::WindowStaysOnTopHint, an);
        show();
    });

    auto *ansicht = menuBar()->addMenu(QStringLiteral("&View"));
    ansicht->addAction(QStringLiteral("&Refresh Now"), QKeySequence(Qt::Key_F5),
                       this, &Hauptfenster::aktualisiere);
    auto *tempo = ansicht->addMenu(QStringLiteral("&Update Speed"));
    auto *gruppe = new QActionGroup(this);
    struct Stufe {
        const char *name;
        int ms;
    };
    // "Paused" ist 0 - der Taktgeber wird dann angehalten, nicht auf
    // einen sehr grossen Wert gestellt. Sonst liefe irgendwann doch eine
    // Messung durch, und der Verlauf haette eine Luecke, die niemand
    // erklaeren kann.
    for (const Stufe &s : {Stufe{"&High", 500}, Stufe{"&Normal", 1000},
                           Stufe{"&Low", 4000}, Stufe{"&Paused", 0}}) {
        auto *eintrag = tempo->addAction(QString::fromLatin1(s.name));
        eintrag->setCheckable(true);
        eintrag->setChecked(s.ms == 1000);
        gruppe->addAction(eintrag);
        connect(eintrag, &QAction::triggered, this,
                [this, ms = s.ms] { setzeTakt(ms); });
    }

    auto *hilfe = menuBar()->addMenu(QStringLiteral("&Help"));
    hilfe->addAction(QStringLiteral("&About Task Manager"), this,
                     &Hauptfenster::ueberDasProgramm);
}

void Hauptfenster::neueAufgabe()
{
    bool bestaetigt = false;
    const QString befehl = QInputDialog::getText(
        this, QStringLiteral("Create New Task"),
        QStringLiteral("Type the name of a program, and Task Manager will "
                       "open it for you."),
        QLineEdit::Normal, QString(), &bestaetigt);
    if (!bestaetigt || befehl.trimmed().isEmpty()) {
        return;
    }
    // Ueber die Shell, damit auch "kate datei.txt" funktioniert. Der
    // Eingabe vertrauen wir dabei: Wer hier tippt, sitzt ohnehin schon
    // vor der Sitzung und koennte dasselbe im Terminal tun.
    if (!QProcess::startDetached(QStringLiteral("/bin/sh"),
                                 {QStringLiteral("-c"), befehl})) {
        QMessageBox::critical(this, QStringLiteral("Task Manager"),
                              QStringLiteral("Cannot start \"%1\".").arg(befehl));
    }
}

void Hauptfenster::setzeTakt(int millisekunden)
{
    if (millisekunden <= 0) {
        m_takt->stop();
        return;
    }
    m_takt->setInterval(millisekunden);
    m_takt->start();
}

void Hauptfenster::ueberDasProgramm()
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

QWidget *Hauptfenster::baueProzessseite()
{
    auto *seite = new QWidget(this);
    auto *lage = new QVBoxLayout(seite);
    lage->setContentsMargins(6, 6, 6, 6);

    m_prozesse = new Prozessmodell(this);
    m_prozessfilter = new QSortFilterProxyModel(this);
    m_prozessfilter->setSourceModel(m_prozesse);
    m_prozessfilter->setSortRole(Qt::UserRole);
    m_prozessfilter->setDynamicSortFilter(true);
    // Ohne das stuenden alle grossgeschriebenen Namen vor allen
    // kleingeschriebenen - ModemManager vor accounts-daemon. Nach
    // ASCII-Werten ist das richtig, fuer den Leser ist es Unsinn.
    m_prozessfilter->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_prozessansicht = new QTableView(seite);
    m_prozessansicht->setModel(m_prozessfilter);
    m_prozessansicht->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_prozessansicht->setSelectionMode(QAbstractItemView::SingleSelection);
    m_prozessansicht->setSortingEnabled(true);
    m_prozessansicht->sortByColumn(Prozessmodell::SpalteName,
                                   Qt::AscendingOrder);
    m_prozessansicht->setShowGrid(false);
    m_prozessansicht->setAlternatingRowColors(false);
    m_prozessansicht->verticalHeader()->hide();
    m_prozessansicht->horizontalHeader()->setStretchLastSection(true);
    m_prozessansicht->verticalHeader()->setDefaultSectionSize(
        m_prozessansicht->fontMetrics().height() + 4);
    m_prozessansicht->setColumnWidth(Prozessmodell::SpalteName, 150);
    m_prozessansicht->setColumnWidth(Prozessmodell::SpalteBenutzer, 90);
    m_prozessansicht->setColumnWidth(Prozessmodell::SpalteCpu, 40);
    lage->addWidget(m_prozessansicht);

    auto *unten = new QHBoxLayout;
    m_fremde = new QCheckBox(QStringLiteral("Show processes from all users"),
                             seite);
    m_fremde->setChecked(true);
    connect(m_fremde, &QCheckBox::toggled, this, &Hauptfenster::aktualisiere);
    unten->addWidget(m_fremde);
    unten->addStretch();

    auto *beenden = new QPushButton(QStringLiteral("End Process"), seite);
    connect(beenden, &QPushButton::clicked, this,
            &Hauptfenster::beendeAuswahl);
    unten->addWidget(beenden);
    lage->addLayout(unten);

    return seite;
}

QWidget *Hauptfenster::baueDiensteseite()
{
    auto *seite = new QWidget(this);
    auto *lage = new QVBoxLayout(seite);
    lage->setContentsMargins(6, 6, 6, 6);

    m_dienste = new Dienstemodell(this);
    m_dienstefilter = new QSortFilterProxyModel(this);
    m_dienstefilter->setSourceModel(m_dienste);
    m_dienstefilter->setSortRole(Qt::UserRole);
    m_dienstefilter->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_diensteansicht = new QTableView(seite);
    m_diensteansicht->setModel(m_dienstefilter);
    m_diensteansicht->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_diensteansicht->setSelectionMode(QAbstractItemView::SingleSelection);
    m_diensteansicht->setSortingEnabled(true);
    m_diensteansicht->sortByColumn(Dienstemodell::SpalteName,
                                   Qt::AscendingOrder);
    m_diensteansicht->setShowGrid(false);
    m_diensteansicht->verticalHeader()->hide();
    m_diensteansicht->horizontalHeader()->setStretchLastSection(true);
    m_diensteansicht->verticalHeader()->setDefaultSectionSize(
        m_diensteansicht->fontMetrics().height() + 4);
    m_diensteansicht->setColumnWidth(Dienstemodell::SpalteName, 170);
    m_diensteansicht->setColumnWidth(Dienstemodell::SpalteBeschreibung, 150);
    lage->addWidget(m_diensteansicht);

    return seite;
}

void Hauptfenster::merkeAuswahl()
{
    m_gemerktePid = -1;
    const QModelIndexList gewaehlt =
        m_prozessansicht->selectionModel()->selectedRows();
    if (gewaehlt.isEmpty()) {
        return;
    }
    const QModelIndex quelle = m_prozessfilter->mapToSource(gewaehlt.first());
    if (const Prozess *p = m_prozesse->prozess(quelle.row())) {
        m_gemerktePid = p->pid;
    }
}

void Hauptfenster::stelleAuswahlHer()
{
    if (m_gemerktePid < 0) {
        return;
    }
    for (int zeile = 0; zeile < m_prozesse->anzahl(); ++zeile) {
        const Prozess *p = m_prozesse->prozess(zeile);
        if (!p || p->pid != m_gemerktePid) {
            continue;
        }
        const QModelIndex ziel =
            m_prozessfilter->mapFromSource(m_prozesse->index(zeile, 0));
        if (ziel.isValid()) {
            m_prozessansicht->selectionModel()->select(
                ziel, QItemSelectionModel::ClearAndSelect
                          | QItemSelectionModel::Rows);
            m_prozessansicht->setCurrentIndex(ziel);
        }
        return;
    }
}

void Hauptfenster::aktualisiere()
{
    // Die Auswahl ueberlebt einen Neuaufbau des Modells nicht. Also
    // vorher die PID merken und hinterher wieder aufsuchen - der Nutzer
    // soll waehrend des Zielens auf "End Process" nicht die Zeile
    // verlieren, weil irgendwo ein Prozess gestartet ist.
    merkeAuswahl();
    m_prozesse->aktualisiere(m_fremde && m_fremde->isChecked());
    stelleAuswahlHer();

    m_standProzesse->setText(
        QStringLiteral("Processes: %1").arg(m_prozesse->anzahl()));
    m_standCpu->setText(QStringLiteral("CPU Usage: %1%")
                            .arg(qRound(m_prozesse->gesamtlast())));
    m_standSpeicher->setText(QStringLiteral("Physical Memory: %1%")
                                 .arg(qRound(m_prozesse->speicherlast())));

    // Dieselben Zahlen weiterreichen, statt sie dort noch einmal zu
    // messen - sonst stuenden im selben Fenster zwei Werte fuer
    // dieselbe Groesse, die sich um ein Prozent unterscheiden.
    if (m_leistung) {
        m_leistung->setzeWerte(m_prozesse->gesamtlast(),
                               m_prozesse->speicherlast(),
                               m_prozesse->speicherBelegt(),
                               m_prozesse->speicherGesamt());
    }
}

void Hauptfenster::beendeAuswahl()
{
    merkeAuswahl();
    if (m_gemerktePid < 0) {
        return;
    }

    int zeile = -1;
    for (int i = 0; i < m_prozesse->anzahl(); ++i) {
        if (m_prozesse->prozess(i)->pid == m_gemerktePid) {
            zeile = i;
            break;
        }
    }
    if (zeile < 0) {
        return;
    }
    const Prozess p = *m_prozesse->prozess(zeile);

    const auto antwort = QMessageBox::warning(
        this, QStringLiteral("Task Manager"),
        QStringLiteral("Terminating a process can cause undesired results "
                       "including loss of data and system instability.\n\n"
                       "Are you sure you want to terminate \"%1\" (PID %2)?")
            .arg(p.name)
            .arg(p.pid),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (antwort != QMessageBox::Yes) {
        return;
    }

    // SIGTERM, nicht SIGKILL: Der Prozess soll aufraeumen duerfen. Der
    // Windows-Taskmanager ist an dieser Stelle brutaler, aber unter
    // Linux ist ein nicht geschlossenes Dateisystemjournal ein Preis,
    // den niemand fuer einen Mausklick zahlen will.
    if (p.eigen) {
        if (::kill(p.pid, SIGTERM) == 0) {
            aktualisiere();
            return;
        }
    }

    // Fremder Prozess oder kein Recht: ueber pkexec, das den
    // Authentifizierungsdialog des Systems oeffnet. Kein eigener
    // Polkit-Code - dafuer gibt es das Werkzeug.
    QProcess erhoeht;
    erhoeht.start(QStringLiteral("pkexec"),
                  {QStringLiteral("kill"), QStringLiteral("-TERM"),
                   QString::number(p.pid)});
    if (!erhoeht.waitForFinished(30000) || erhoeht.exitCode() != 0) {
        QMessageBox::critical(
            this, QStringLiteral("Task Manager"),
            QStringLiteral("The process \"%1\" could not be terminated.")
                .arg(p.name));
    }
    aktualisiere();
}
