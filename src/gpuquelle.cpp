#include "gpuquelle.h"

#include <QDir>
#include <QFile>

#include <dlfcn.h>

namespace {

// Die NVML-Strukturen, selbst deklariert. Der Header liegt nur im
// CUDA-Werkzeugkasten, den niemand fuer einen Taskmanager installieren
// soll. Beide Verbunde sind seit NVML 1.0 unveraendert; gegen die
// Bibliothek geprueft (RTX 5080, Treiber 595): der Wert deckt sich aufs
// Prozent mit nvidia-smi.
struct NvmlAuslastung {
    unsigned int gpu;
    unsigned int speicherbus;
};

struct NvmlSpeicher {
    unsigned long long gesamt;
    unsigned long long frei;
    unsigned long long belegt;
};

using NvmlInit = int (*)();
using NvmlGriff = int (*)(unsigned int, void **);
using NvmlName = int (*)(void *, char *, unsigned int);
using NvmlLast = int (*)(void *, NvmlAuslastung *);
using NvmlSpeicherInfo = int (*)(void *, NvmlSpeicher *);

} // namespace

Gpuquelle::Gpuquelle()
{
    if (sucheNvidia()) {
        m_art = Nvidia;
        return;
    }
    if (sucheAmd()) {
        m_art = Amd;
        return;
    }
}

Gpuquelle::~Gpuquelle()
{
    if (m_bibliothek) {
        // nvmlShutdown fehlt hier bewusst: Das Programm endet ohnehin,
        // und der Aufruf hat auf manchen Treiberversionen mehr
        // Sekundenbruchteile gekostet als das ganze Beenden.
        dlclose(m_bibliothek);
    }
}

bool Gpuquelle::sucheNvidia()
{
    m_bibliothek = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
    if (!m_bibliothek) {
        return false;
    }

    auto init = reinterpret_cast<NvmlInit>(dlsym(m_bibliothek, "nvmlInit_v2"));
    auto griff = reinterpret_cast<NvmlGriff>(
        dlsym(m_bibliothek, "nvmlDeviceGetHandleByIndex_v2"));
    m_last_fn = dlsym(m_bibliothek, "nvmlDeviceGetUtilizationRates");
    m_speicher_fn = dlsym(m_bibliothek, "nvmlDeviceGetMemoryInfo");

    if (!init || !griff || !m_last_fn || init() != 0) {
        dlclose(m_bibliothek);
        m_bibliothek = nullptr;
        return false;
    }

    // Karte 0. Mehrere Karten waeren mehrere Reiter - dafuer ist dieses
    // Programm nicht gedacht.
    if (griff(0, &m_geraet) != 0) {
        dlclose(m_bibliothek);
        m_bibliothek = nullptr;
        return false;
    }

    auto name = reinterpret_cast<NvmlName>(dlsym(m_bibliothek, "nvmlDeviceGetName"));
    char puffer[96] = {0};
    if (name && name(m_geraet, puffer, sizeof(puffer)) == 0) {
        m_name = QString::fromLatin1(puffer);
    } else {
        m_name = QStringLiteral("NVIDIA");
    }
    return true;
}

bool Gpuquelle::sucheAmd()
{
    QDir drm(QStringLiteral("/sys/class/drm"));
    const QStringList karten =
        drm.entryList({QStringLiteral("card[0-9]")}, QDir::Dirs);
    for (const QString &karte : karten) {
        const QString pfad = drm.filePath(karte)
                             + QStringLiteral("/device/gpu_busy_percent");
        if (!QFile::exists(pfad)) {
            continue;
        }
        m_amdpfad = pfad;
        QFile kennung(drm.filePath(karte) + QStringLiteral("/device/uevent"));
        m_name = QStringLiteral("AMD");
        if (kennung.open(QIODevice::ReadOnly)) {
            for (const QByteArray &zeile : kennung.readAll().split('\n')) {
                if (zeile.startsWith("DRIVER=")) {
                    m_name = QStringLiteral("AMD (")
                             + QString::fromLatin1(zeile.mid(7)) + QLatin1Char(')');
                    break;
                }
            }
        }
        return true;
    }
    return false;
}

void Gpuquelle::miss()
{
    if (m_art == Nvidia) {
        NvmlAuslastung a{0, 0};
        if (reinterpret_cast<NvmlLast>(m_last_fn)(m_geraet, &a) == 0) {
            m_last = a.gpu;
        }
        if (m_speicher_fn) {
            NvmlSpeicher s{0, 0, 0};
            if (reinterpret_cast<NvmlSpeicherInfo>(m_speicher_fn)(m_geraet, &s) == 0) {
                m_belegt = static_cast<qint64>(s.belegt / 1024);
                m_gesamt = static_cast<qint64>(s.gesamt / 1024);
            }
        }
        return;
    }

    if (m_art == Amd) {
        QFile datei(m_amdpfad);
        if (datei.open(QIODevice::ReadOnly)) {
            // readAll, nicht readLine mit atEnd - sysfs meldet wie procfs
            // die Groesse 0.
            m_last = datei.readAll().trimmed().toDouble();
        }
    }
}
