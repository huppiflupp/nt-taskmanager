#include "leistungsseite.h"

#include "anzeigen.h"
#include "gpuquelle.h"

#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

#include <unistd.h>

namespace {
// Der Name des Prozessors, wie ihn /proc/cpuinfo fuehrt. Steht dort in
// jedem Kernblock derselbe - der erste genuegt.
//
// Die Herstellerzusaetze fliegen raus: "Intel(R) Core(TM) Ultra 5 245K"
// wird zu "Intel Core Ultra 5 245K". Im Original stand an dieser Stelle
// auch nur, was man einem Menschen sagen wuerde.
QString prozessorname()
{
    QFile datei(QStringLiteral("/proc/cpuinfo"));
    if (!datei.open(QIODevice::ReadOnly)) {
        return {};
    }
    for (const QByteArray &zeile : datei.readAll().split('\n')) {
        if (!zeile.startsWith("model name")) {
            continue;
        }
        const int trenner = zeile.indexOf(':');
        if (trenner < 0) {
            break;
        }
        QString name = QString::fromLatin1(zeile.mid(trenner + 1)).trimmed();
        name.remove(QStringLiteral("(R)"));
        name.remove(QStringLiteral("(TM)"));
        name.remove(QStringLiteral("(tm)"));
        return name.simplified();
    }
    // Auf ARM gibt es kein model name. Dann lieber nichts als eine
    // erfundene Bezeichnung.
    return {};
}
} // namespace

Leistungsseite::Leistungsseite(QWidget *eltern)
    : QWidget(eltern)
    , m_gpuquelle(new Gpuquelle)
{
    auto *lage = new QVBoxLayout(this);
    lage->setContentsMargins(6, 6, 6, 6);
    lage->setSpacing(6);

    m_cpu = baueZeile(QStringLiteral("CPU Usage"),
                      QStringLiteral("CPU Usage History"), this, lage);
    const QString prozessor = prozessorname();
    const int kerne = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (!prozessor.isEmpty()) {
        m_cpu.zusatz->setText(kerne > 1
                                  ? QStringLiteral("%1  %2 cores")
                                        .arg(prozessor)
                                        .arg(kerne)
                                  : prozessor);
    }

    const QString gpuname = m_gpuquelle->vorhanden()
                                ? QStringLiteral("GPU Usage")
                                : QStringLiteral("GPU Usage (no adapter)");
    m_gpu = baueZeile(gpuname, QStringLiteral("GPU Usage History"), this, lage);
    if (m_gpuquelle->vorhanden()) {
        m_gpu.zusatz->setText(m_gpuquelle->name());
    } else {
        // Kein Balken, der auf null klebt, und keine Kurve, die eine
        // Messung vortaeuscht - lieber ehrlich leer.
        m_gpu.balken->setEnabled(false);
        m_gpu.verlauf->setEnabled(false);
        m_gpu.zusatz->setText(QStringLiteral("none detected"));
    }

    m_ram = baueZeile(QStringLiteral("Mem Usage"),
                      QStringLiteral("Memory Usage History"), this, lage);
}

Leistungsseite::~Leistungsseite()
{
    delete m_gpuquelle;
}

Leistungsseite::Zeile Leistungsseite::baueZeile(const QString &titel,
                                                const QString &verlaufstitel,
                                                QWidget *eltern,
                                                QLayout *lage)
{
    Zeile z;

    auto *reihe = new QHBoxLayout;
    reihe->setSpacing(6);

    auto *linkeGruppe = new QGroupBox(titel, eltern);
    auto *links = new QVBoxLayout(linkeGruppe);
    links->setContentsMargins(6, 4, 6, 4);
    z.balken = new Balken(linkeGruppe);
    links->addWidget(z.balken, 1, Qt::AlignHCenter);
    z.wert = new QLabel(QStringLiteral("0%"), linkeGruppe);
    z.wert->setAlignment(Qt::AlignCenter);
    links->addWidget(z.wert);
    reihe->addWidget(linkeGruppe, 0);

    auto *rechteGruppe = new QGroupBox(verlaufstitel, eltern);
    auto *rechts = new QVBoxLayout(rechteGruppe);
    rechts->setContentsMargins(6, 4, 6, 4);
    z.verlauf = new Verlauf(rechteGruppe);
    rechts->addWidget(z.verlauf, 1);
    z.zusatz = new QLabel(rechteGruppe);
    z.zusatz->setAlignment(Qt::AlignRight);
    rechts->addWidget(z.zusatz);
    reihe->addWidget(rechteGruppe, 1);

    lage->addItem(reihe);
    return z;
}

void Leistungsseite::setzeWerte(double cpu, double speicher,
                                qint64 speicherBelegtKb,
                                qint64 speicherGesamtKb)
{
    m_cpu.balken->setzeWert(cpu);
    m_cpu.verlauf->schiebe(cpu);
    m_cpu.wert->setText(QStringLiteral("%1%").arg(qRound(cpu)));

    if (m_gpuquelle->vorhanden()) {
        m_gpuquelle->miss();
        const double last = m_gpuquelle->last();
        m_gpu.balken->setzeWert(last);
        m_gpu.verlauf->schiebe(last);
        m_gpu.wert->setText(QStringLiteral("%1%").arg(qRound(last)));
        if (m_gpuquelle->speicherGesamt() > 0) {
            m_gpu.zusatz->setText(
                QStringLiteral("%1  %2 / %3 MB")
                    .arg(m_gpuquelle->name())
                    .arg(QLocale().toString(m_gpuquelle->speicherBelegt() / 1024))
                    .arg(QLocale().toString(m_gpuquelle->speicherGesamt() / 1024)));
        }
    }

    m_ram.balken->setzeWert(speicher);
    m_ram.verlauf->schiebe(speicher);
    m_ram.wert->setText(QStringLiteral("%1%").arg(qRound(speicher)));
    if (speicherGesamtKb > 0) {
        m_ram.zusatz->setText(
            QStringLiteral("%1 / %2 MB")
                .arg(QLocale().toString(speicherBelegtKb / 1024))
                .arg(QLocale().toString(speicherGesamtKb / 1024)));
    }
}
