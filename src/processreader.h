// Reads the process table from /proc.
//
// No libksysguard, no procps: what is needed here are four files per
// process, and their formats are fixed in proc(5). The library would be
// more dependency than gain.
#pragma once

#include <QHash>
#include <QString>
#include <QVector>

struct Process {
    int pid = 0;
    QString name;      // taken from the exe symlink, comm as a fallback
    QString user;      // resolved through getpwuid, with a cache
    double cpu = 0.0;  // per cent, across all cores as NT counted it
    qint64 memory = 0; // resident set size in kilobytes
    bool own = false;  // runs under our uid - decides how to terminate it
};

class ProcessReader {
public:
    ProcessReader();

    // Reads all of /proc once. CPU load follows from the distance to the
    // previous call, so the first call reports 0.0 everywhere - that is
    // correct, not a gap.
    QVector<Process> read();

    // Total load and memory usage. All of it is produced by read(); what
    // follows are plain queries. The Performance tab and the status bar
    // must see the same measurement, not two of their own.
    double totalCpu() const { return m_totalCpu; }
    double memoryUsage() const;
    qint64 memoryUsed() const { return m_memoryUsed; }
    qint64 memoryTotal() const { return m_memoryTotal; }

private:
    QString userName(uint uid);
    void readMemory();

    struct Counter {
        qint64 ticks = 0; // utime + stime at the previous reading
        qint64 stamp = 0;
    };

    QHash<int, Counter> m_previous;
    QHash<uint, QString> m_names; // uid -> name, getpwuid is expensive
    qint64 m_totalPrevious = 0;   // sum of all fields in /proc/stat
    qint64 m_idlePrevious = 0;
    double m_totalCpu = 0.0;
    qint64 m_memoryUsed = 0; // kilobytes
    qint64 m_memoryTotal = 0;
    long m_ticksPerSecond = 100;
    int m_cores = 1;
};
