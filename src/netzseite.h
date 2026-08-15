// Der Reiter "Networking": je Schnittstelle ein Verlauf, darunter die
// Tabelle mit Auslastung, Linkgeschwindigkeit und Zustand.
#pragma once

#include <QHash>
#include <QWidget>

#include "netzquelle.h"

class QGroupBox;
class QTableWidget;
class QVBoxLayout;

class Verlauf;

class Netzseite : public QWidget {
    Q_OBJECT

public:
    explicit Netzseite(QWidget *eltern = nullptr);

    void aktualisiere();

private:
    struct Anzeige {
        QGroupBox *rahmen = nullptr;
        Verlauf *verlauf = nullptr;
        // Hoechste bisher gesehene Rate, fuer Schnittstellen ohne
        // bekannte Linkgeschwindigkeit.
        double spitze = 0.0;
    };

    Anzeige &anzeigeFuer(const Netzadapter &adapter);

    Netzquelle m_quelle;
    QHash<QString, Anzeige> m_anzeigen;
    QVBoxLayout *m_graphen = nullptr;
    QTableWidget *m_tabelle = nullptr;
    bool m_breiten_gesetzt = false;
};
