// Load and memory of the graphics card.
//
// Two ways, because Linux has no single one:
//
//   NVIDIA  through NVML (libnvidia-ml), loaded at runtime. Not linked
//           against - otherwise the program would refuse to start on
//           every machine without the NVIDIA driver, which is most of
//           them.
//   AMD     through /sys/class/drm/cardN/device/gpu_busy_percent. Three
//           lines, present since kernel 4.19.
//
// Intel is missing on purpose. Its utilisation sits behind the i915 PMU,
// which is not readable without elevated privileges - a system monitor
// that asks for a password in order to draw a bar would be the wrong
// answer.
#pragma once

#include <QString>

class GpuReader {
public:
    GpuReader();
    ~GpuReader();

    GpuReader(const GpuReader &) = delete;
    GpuReader &operator=(const GpuReader &) = delete;

    // True when a card was found at all. Otherwise the tab still stands,
    // but shows "no adapter" instead of a bar frozen at zero.
    bool present() const { return m_kind != None; }
    QString name() const { return m_name; }

    // Take one measurement. Values in per cent and kilobytes; -1 means
    // this card does not report it.
    void measure();
    double load() const { return m_load; }
    qint64 memoryUsed() const { return m_used; }
    qint64 memoryTotal() const { return m_total; }

private:
    enum Kind { None, Nvidia, Amd };

    bool findNvidia();
    bool findAmd();

    Kind m_kind = None;
    QString m_name;
    QString m_amdPath;

    void *m_library = nullptr;
    void *m_device = nullptr;
    void *m_loadFn = nullptr;
    void *m_memoryFn = nullptr;

    double m_load = 0.0;
    qint64 m_used = -1;
    qint64 m_total = -1;
};
