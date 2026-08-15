// Die Prozesstabelle als Qt-Modell.
//
// Warum ein eigenes Modell und kein QTreeWidget mit Eintraegen: Die
// Liste wird jede Sekunde neu gelesen. Ein Widget-Baum wuerde dabei
// jedes Mal geleert und neu befuellt - Auswahl weg, Bildlaufposition
// weg, Sortierung weg. Ein Modell tauscht nur die Daten aus und meldet,
// was sich geaendert hat; die Ansicht behaelt ihren Zustand.
#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "prozessquelle.h"

class Prozessmodell : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Spalte {
        SpalteName = 0,
        SpalteBenutzer,
        SpalteCpu,
        SpalteSpeicher,
        SpaltePid,
        SpaltenZahl
    };

    explicit Prozessmodell(QObject *eltern = nullptr);

    int rowCount(const QModelIndex &eltern = {}) const override;
    int columnCount(const QModelIndex &eltern = {}) const override;
    QVariant data(const QModelIndex &index, int rolle) const override;
    QVariant headerData(int abschnitt, Qt::Orientation richtung,
                        int rolle) const override;

    // Neu einlesen. fremde_zeigen entscheidet, ob Prozesse anderer
    // Benutzer in der Liste stehen - das Haekchen unten im Fenster.
    void aktualisiere(bool fremde_zeigen);

    const Prozess *prozess(int zeile) const;
    double gesamtlast() const { return m_quelle.gesamtlast(); }
    double speicherlast() const { return m_quelle.speicherlast(); }
    int anzahl() const { return static_cast<int>(m_zeilen.size()); }

private:
    Prozessquelle m_quelle;
    QVector<Prozess> m_zeilen;
};
