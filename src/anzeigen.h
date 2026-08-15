// Die beiden gezeichneten Anzeigen: Balken und Verlauf.
//
// Das ist die eine Stelle, an der dieses Programm doch selbst zeichnet -
// Balken und Kurven liefert kein Widget-Stil. Der Rahmen kommt trotzdem
// vom Stil (QStyle::PE_Frame), damit die Vertiefung dieselbe ist wie an
// jeder anderen Tabelle im Fenster.
//
// Gruen auf Schwarz ist Absicht und wechselt NICHT mit dem Farbschema.
// Windows hat es genauso gehalten: die Fensterfarben folgten dem
// eingestellten Schema, die Anzeigen im Taskmanager blieben gruen. Wer
// sie einfaerbte, verlor genau das Bild, das jeder kennt.
#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class Balken : public QWidget {
    Q_OBJECT

public:
    explicit Balken(QWidget *eltern = nullptr);

    void setzeWert(double prozent);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *ereignis) override;

private:
    double m_wert = 0.0;
};

class Verlauf : public QWidget {
    Q_OBJECT

public:
    explicit Verlauf(QWidget *eltern = nullptr);

    void schiebe(double prozent);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *ereignis) override;

private:
    // Feste Laenge, ringfoermig beschrieben. Der Verlauf zeigt immer
    // dieselbe Zeitspanne, egal wie breit das Fenster gerade ist -
    // sonst aenderte sich beim Ziehen am Rand die Bedeutung der Kurve.
    static constexpr int Punkte = 120;

    QVector<double> m_werte;
    int m_naechster = 0;
    int m_gefuellt = 0;
    // Das Raster wandert mit den Daten nach links, wie im Original. Ohne
    // das steht die Kurve scheinbar still, wenn die Last gleich bleibt.
    int m_versatz = 0;
};
