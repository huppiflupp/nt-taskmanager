#include "gpureader.h"

#include <QDir>
#include <QFile>

#include <dlfcn.h>

namespace {

// The NVML structs, declared here. The header only ships with the CUDA
// toolkit, which nobody should install for a task manager. Both structs
// have been unchanged since NVML 1.0; checked against the library on
// this machine (RTX 5080, driver 595): the value matches nvidia-smi to
// the per cent.
struct NvmlUtilisation {
    unsigned int gpu;
    unsigned int memoryBus;
};

struct NvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

using NvmlInit = int (*)();
using NvmlHandle = int (*)(unsigned int, void **);
using NvmlName = int (*)(void *, char *, unsigned int);
using NvmlLoad = int (*)(void *, NvmlUtilisation *);
using NvmlMemoryInfo = int (*)(void *, NvmlMemory *);

} // namespace

GpuReader::GpuReader()
{
    if (findNvidia()) {
        m_kind = Nvidia;
        return;
    }
    if (findAmd()) {
        m_kind = Amd;
        return;
    }
}

GpuReader::~GpuReader()
{
    if (m_library) {
        // nvmlShutdown is deliberately absent: the program is ending
        // anyway, and on some driver versions the call cost more
        // fractions of a second than the whole shutdown.
        dlclose(m_library);
    }
}

bool GpuReader::findNvidia()
{
    m_library = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
    if (!m_library) {
        return false;
    }

    auto init = reinterpret_cast<NvmlInit>(dlsym(m_library, "nvmlInit_v2"));
    auto handle = reinterpret_cast<NvmlHandle>(
        dlsym(m_library, "nvmlDeviceGetHandleByIndex_v2"));
    m_loadFn = dlsym(m_library, "nvmlDeviceGetUtilizationRates");
    m_memoryFn = dlsym(m_library, "nvmlDeviceGetMemoryInfo");

    if (!init || !handle || !m_loadFn || init() != 0) {
        dlclose(m_library);
        m_library = nullptr;
        return false;
    }

    // Card 0. Several cards would mean several tabs, and that is not
    // what this program is for.
    if (handle(0, &m_device) != 0) {
        dlclose(m_library);
        m_library = nullptr;
        return false;
    }

    auto name = reinterpret_cast<NvmlName>(dlsym(m_library, "nvmlDeviceGetName"));
    char buffer[96] = {0};
    if (name && name(m_device, buffer, sizeof(buffer)) == 0) {
        m_name = QString::fromLatin1(buffer);
    } else {
        m_name = QStringLiteral("NVIDIA");
    }
    return true;
}

bool GpuReader::findAmd()
{
    QDir drm(QStringLiteral("/sys/class/drm"));
    const QStringList cards =
        drm.entryList({QStringLiteral("card[0-9]")}, QDir::Dirs);
    for (const QString &card : cards) {
        const QString path =
            drm.filePath(card) + QStringLiteral("/device/gpu_busy_percent");
        if (!QFile::exists(path)) {
            continue;
        }
        m_amdPath = path;
        QFile uevent(drm.filePath(card) + QStringLiteral("/device/uevent"));
        m_name = QStringLiteral("AMD");
        if (uevent.open(QIODevice::ReadOnly)) {
            for (const QByteArray &line : uevent.readAll().split('\n')) {
                if (line.startsWith("DRIVER=")) {
                    m_name = QStringLiteral("AMD (")
                             + QString::fromLatin1(line.mid(7)) + QLatin1Char(')');
                    break;
                }
            }
        }
        return true;
    }
    return false;
}

void GpuReader::measure()
{
    if (m_kind == Nvidia) {
        NvmlUtilisation u{0, 0};
        if (reinterpret_cast<NvmlLoad>(m_loadFn)(m_device, &u) == 0) {
            m_load = u.gpu;
        }
        if (m_memoryFn) {
            NvmlMemory m{0, 0, 0};
            if (reinterpret_cast<NvmlMemoryInfo>(m_memoryFn)(m_device, &m) == 0) {
                m_used = static_cast<qint64>(m.used / 1024);
                m_total = static_cast<qint64>(m.total / 1024);
            }
        }
        return;
    }

    if (m_kind == Amd) {
        QFile file(m_amdPath);
        if (file.open(QIODevice::ReadOnly)) {
            // readAll, not readLine with atEnd - sysfs reports a size of
            // 0 just like procfs.
            m_load = file.readAll().trimmed().toDouble();
        }
    }
}
