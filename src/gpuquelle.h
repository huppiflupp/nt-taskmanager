// Auslastung und Speicher der Grafikkarte.
//
// Zwei Wege, weil es unter Linux keinen einheitlichen gibt:
//
//   NVIDIA  ueber NVML (libnvidia-ml), zur Laufzeit nachgeladen. Nicht
//           gegen die Bibliothek gelinkt - sonst liefe das Programm auf
//           keinem Rechner ohne NVIDIA-Treiber, und das waeren die
//           meisten.
//   AMD     ueber /sys/class/drm/cardN/device/gpu_busy_percent. Drei
//           Zeilen, seit Kernel 4.19 vorhanden.
//
// Intel fehlt mit Absicht. Die Auslastung liegt dort hinter dem
// i915-PMU, das ohne erhoehte Rechte nicht lesbar ist - ein
// Systemmonitor, der nach dem Passwort fragt, um einen Balken zu
// zeichnen, waere die falsche Antwort.
#pragma once

#include <QString>

class Gpuquelle {
public:
    Gpuquelle();
    ~Gpuquelle();

    Gpuquelle(const Gpuquelle &) = delete;
    Gpuquelle &operator=(const Gpuquelle &) = delete;

    // Wahr, wenn ueberhaupt eine Karte gefunden wurde. Sonst bleibt der
    // Reiter zwar stehen, zeigt aber "no adapter" statt eines Balkens,
    // der immer auf null steht.
    bool vorhanden() const { return m_art != Keine; }
    QString name() const { return m_name; }

    // Einmal messen. Werte in Prozent beziehungsweise Kilobyte; -1
    // bedeutet "diese Karte gibt das nicht her".
    void miss();
    double last() const { return m_last; }
    qint64 speicherBelegt() const { return m_belegt; }
    qint64 speicherGesamt() const { return m_gesamt; }

private:
    enum Art { Keine, Nvidia, Amd };

    bool sucheNvidia();
    bool sucheAmd();

    Art m_art = Keine;
    QString m_name;
    QString m_amdpfad;

    void *m_bibliothek = nullptr;
    void *m_geraet = nullptr;
    void *m_last_fn = nullptr;
    void *m_speicher_fn = nullptr;

    double m_last = 0.0;
    qint64 m_belegt = -1;
    qint64 m_gesamt = -1;
};
