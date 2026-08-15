// Der Reiter "Performance": Prozessor, Grafikkarte und Arbeitsspeicher
// als Balken mit Verlauf daneben.
//
// Die Aufteilung ist die des Originals - links der Balken mit dem
// Augenblickswert, rechts der Verlauf ueber die letzten zwei Minuten,
// beides in einer beschrifteten Gruppe. Nur eine Zeile ist dazugekommen,
// die es 1996 nicht gab: die Grafikkarte.
#pragma once

#include <QWidget>

class QLabel;

class Balken;
class Gpuquelle;
class Verlauf;

class Leistungsseite : public QWidget {
    Q_OBJECT

public:
    explicit Leistungsseite(QWidget *eltern = nullptr);
    ~Leistungsseite() override;

    // Wird vom Hauptfenster im Sekundentakt gefuettert. Prozessorlast
    // und Speicher liegen dort ohnehin schon vor - sie ein zweites Mal
    // aus /proc zu lesen, waere doppelte Arbeit und ergaebe obendrein
    // zwei leicht verschiedene Zahlen im selben Fenster.
    void setzeWerte(double cpu, double speicher, qint64 speicherBelegtKb,
                    qint64 speicherGesamtKb);

private:
    struct Zeile {
        Balken *balken = nullptr;
        Verlauf *verlauf = nullptr;
        QLabel *wert = nullptr;
        QLabel *zusatz = nullptr;
    };

    Zeile baueZeile(const QString &titel, const QString &verlaufstitel,
                    QWidget *eltern, QLayout *lage);

    Zeile m_cpu;
    Zeile m_gpu;
    Zeile m_ram;
    Gpuquelle *m_gpuquelle = nullptr;
};
