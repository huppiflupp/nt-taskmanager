// Das Fenster.
//
// Es zeichnet nichts selbst und setzt keine Farben. Alles, was hier nach
// Windows NT aussieht, kommt vom eingestellten Widget-Stil und vom
// Farbschema - siehe README. Ein Stylesheet oder eine eigene Palette
// waere genau der Fehler, den das Programm nicht machen soll: es saehe
// dann unter jedem anderen Design falsch aus.
#pragma once

#include <QMainWindow>

class QCheckBox;
class QLabel;
class QSortFilterProxyModel;
class QTableView;
class QTabWidget;
class QTimer;

class Dienstemodell;
class Prozessmodell;

class Hauptfenster : public QMainWindow {
    Q_OBJECT

public:
    explicit Hauptfenster(QWidget *eltern = nullptr);

    // Schreibt die Kennzahlen auf die Standardausgabe. Fuer --bild, wo
    // niemand hinsieht, aber jemand nachrechnen koennen soll.
    void melde() const;
    void zeigeReiter(int nummer);

private Q_SLOTS:
    void aktualisiere();
    void beendeAuswahl();

private:
    QWidget *baueProzessseite();
    QWidget *baueDiensteseite();
    void merkeAuswahl();
    void stelleAuswahlHer();

    Prozessmodell *m_prozesse = nullptr;
    QSortFilterProxyModel *m_prozessfilter = nullptr;
    QTableView *m_prozessansicht = nullptr;
    QCheckBox *m_fremde = nullptr;

    Dienstemodell *m_dienste = nullptr;
    QSortFilterProxyModel *m_dienstefilter = nullptr;
    QTableView *m_diensteansicht = nullptr;

    QLabel *m_standProzesse = nullptr;
    QLabel *m_standCpu = nullptr;
    QLabel *m_standSpeicher = nullptr;

    QTabWidget *m_reiter = nullptr;
    QTimer *m_takt = nullptr;
    int m_gemerktePid = -1;
};
