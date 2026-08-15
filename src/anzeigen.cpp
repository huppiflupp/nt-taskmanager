#include "anzeigen.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include <cmath>

namespace {
// Die Farben aus dem Original. Nicht aus der Palette geholt - siehe
// Kommentar in der Kopfdatei.
const QColor Grund(0, 0, 0);
// Dunkler als die Kurve, und zwar deutlich: Das Raster soll die
// Skala andeuten, nicht mit der Messung um Aufmerksamkeit
// streiten. Bei (0,100,0) wirkte die Flaeche flaechig gruen und
// die Kurve verschwand darin.
const QColor Gitter(0, 60, 0);
const QColor Linie(0, 255, 0);
const QColor Segment(0, 220, 0);

// Ein Segment des Balkens: zwei Pixel hoch, ein Pixel Luecke. So sah der
// Balken in NT aus, und das ist auch der Grund, warum er nicht einfach
// ein gefuelltes Rechteck ist.
constexpr int SegmentHoehe = 2;
constexpr int SegmentLuecke = 1;

// Kantenlaenge einer Rasterzelle im Verlauf.
constexpr double Zellenbreite = 14.0;

void zeichneRahmen(QPainter *maler, QWidget *widget)
{
    QStyleOptionFrame rahmen;
    rahmen.initFrom(widget);
    rahmen.frameShape = QFrame::Panel;
    rahmen.lineWidth = 1;
    rahmen.midLineWidth = 0;
    rahmen.state |= QStyle::State_Sunken;
    widget->style()->drawPrimitive(QStyle::PE_Frame, &rahmen, maler, widget);
}
} // namespace

// ── Balken ───────────────────────────────────────────────────────────

Balken::Balken(QWidget *eltern)
    : QWidget(eltern)
{
}

void Balken::setzeWert(double prozent)
{
    m_wert = qBound(0.0, prozent, 100.0);
    update();
}

QSize Balken::sizeHint() const
{
    return {28, 110};
}

QSize Balken::minimumSizeHint() const
{
    return {20, 60};
}

void Balken::paintEvent(QPaintEvent *)
{
    QPainter maler(this);
    zeichneRahmen(&maler, this);

    const QRect innen = rect().adjusted(2, 2, -2, -2);
    maler.fillRect(innen, Grund);

    const int schritt = SegmentHoehe + SegmentLuecke;
    const int segmente_gesamt = innen.height() / schritt;
    const int segmente_an = qRound(segmente_gesamt * m_wert / 100.0);

    for (int i = 0; i < segmente_an; ++i) {
        // Von unten nach oben fuellen.
        const int y = innen.bottom() - (i + 1) * schritt + SegmentLuecke;
        maler.fillRect(innen.left(), y, innen.width(), SegmentHoehe, Segment);
    }
}

// ── Verlauf ──────────────────────────────────────────────────────────

Verlauf::Verlauf(QWidget *eltern)
    : QWidget(eltern)
{
    m_werte.resize(Punkte);
}

void Verlauf::schiebe(double prozent)
{
    m_werte[m_naechster] = qBound(0.0, prozent, 100.0);
    m_naechster = (m_naechster + 1) % Punkte;
    if (m_gefuellt < Punkte) {
        ++m_gefuellt;
    }
    m_versatz = (m_versatz + 1) % Punkte;
    update();
}

QSize Verlauf::sizeHint() const
{
    return {180, 110};
}

QSize Verlauf::minimumSizeHint() const
{
    return {80, 60};
}

void Verlauf::paintEvent(QPaintEvent *)
{
    QPainter maler(this);
    zeichneRahmen(&maler, this);

    const QRect innen = rect().adjusted(2, 2, -2, -2);
    if (innen.width() < 4 || innen.height() < 4) {
        return;
    }
    maler.fillRect(innen, Grund);
    maler.setClipRect(innen);

    // Der waagerechte Abstand zweier Messwerte. Die 120 Werte fuellen
    // IMMER die ganze Breite, egal wie breit das Fenster gerade ist.
    //
    // Ein Wert je Pixelspalte war der erste Versuch, und der war falsch:
    // In einem 330 Pixel breiten Feld haetten 120 Werte nur das rechte
    // Drittel gefuellt, die uebrigen zwei Drittel waeren fuer immer leer
    // geblieben. Der Verlauf zeigt eine feste Zeitspanne, keine feste
    // Pixelzahl.
    const double schritt = double(innen.width() - 1) / (Punkte - 1);

    // Raster. Die senkrechten Linien wandern mit, die waagerechten
    // stehen fest - genau wie im Original, wo die Zeitachse laeuft und
    // die Prozentachse nicht.
    maler.setPen(Gitter);
    const double versatz = std::fmod(m_versatz * schritt, Zellenbreite);
    for (double x = innen.right() - versatz; x > innen.left(); x -= Zellenbreite) {
        maler.drawLine(QPointF(x, innen.top()), QPointF(x, innen.bottom()));
    }
    for (int i = 1; i < 6; ++i) {
        const int y = innen.top() + innen.height() * i / 6;
        maler.drawLine(innen.left(), y, innen.right(), y);
    }

    if (m_gefuellt < 2) {
        return;
    }

    // Die Kurve laeuft von rechts nach links: der neueste Wert steht am
    // rechten Rand. Ein Punkt je Pixelspalte, hoechstens so viele, wie
    // gemessen wurden.
    const int sichtbar = m_gefuellt;
    QPolygonF kurve;
    kurve.reserve(sichtbar);
    for (int i = 0; i < sichtbar; ++i) {
        // i = 0 ist der neueste Wert.
        const int stelle = (m_naechster - 1 - i + 2 * Punkte) % Punkte;
        const double wert = m_werte.at(stelle);
        const double x = innen.right() - i * schritt;
        const double y = innen.bottom() - (innen.height() - 1) * wert / 100.0;
        kurve << QPointF(x, y);
    }

    maler.setPen(QPen(Linie, 1));
    maler.drawPolyline(kurve);
}
