#include "processreader.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <pwd.h>
#include <unistd.h>

ProcessReader::ProcessReader()
{
    m_ticksPerSecond = sysconf(_SC_CLK_TCK);
    if (m_ticksPerSecond <= 0) {
        m_ticksPerSecond = 100;
    }
    m_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (m_cores <= 0) {
        m_cores = 1;
    }
}

QString ProcessReader::userName(uint uid)
{
    auto hit = m_names.constFind(uid);
    if (hit != m_names.constEnd()) {
        return *hit;
    }
    // getpwuid goes over the network when the directory does. At 500
    // processes per refresh that would be the most expensive thing in
    // the whole program - hence the cache.
    QString name = QString::number(uid);
    if (const passwd *entry = getpwuid(uid)) {
        name = QString::fromLocal8Bit(entry->pw_name);
    }
    m_names.insert(uid, name);
    return name;
}

// The name in /proc/<pid>/stat is wrapped in parentheses and may itself
// contain parentheses and spaces ("(Web Content)"). So parsing continues
// after the LAST closing parenthesis instead of splitting on spaces - a
// mistake that only shows once someone starts a process with a space in
// its name.
static bool ticksFromStat(const QByteArray &content, qint64 *ticks)
{
    const int close = content.lastIndexOf(')');
    if (close < 0) {
        return false;
    }
    const QList<QByteArray> fields = content.mid(close + 2).split(' ');
    // From here field 0 is state (field 3 in proc(5)). utime is field
    // 14, stime field 15 - index 11 and 12 in this counting.
    if (fields.size() < 13) {
        return false;
    }
    *ticks = fields.at(11).toLongLong() + fields.at(12).toLongLong();
    return true;
}

QVector<Process> ProcessReader::read()
{
    QVector<Process> list;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const uint ownUid = getuid();

    QHash<int, Counter> current;
    QDir proc(QStringLiteral("/proc"));
    const QStringList entries =
        proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);

    for (const QString &entry : entries) {
        bool isNumber = false;
        const int pid = entry.toInt(&isNumber);
        if (!isNumber) {
            continue;
        }

        const QString base = QStringLiteral("/proc/") + entry;

        // Processes vanish while we read. Every file that fails to open
        // is therefore an ordinary case, not an error.
        QFile stat(base + QStringLiteral("/stat"));
        if (!stat.open(QIODevice::ReadOnly)) {
            continue;
        }
        qint64 ticks = 0;
        if (!ticksFromStat(stat.readAll(), &ticks)) {
            continue;
        }

        Process p;
        p.pid = pid;

        // The name comes from the executable path, not from comm.
        //
        // comm is capped at 15 characters - a kernel limit that has
        // always been there. plasma-systemmonitor turns into
        // "plasma-systemmo" there, and the name column is the most
        // important column of this program. The exe symlink points at
        // the real file, without a length limit.
        //
        // No cache for it: readlink is a syscall that never touches the
        // disk, 500 of them per second do not register. A cache would
        // have a real problem instead - pids get reused, and a new
        // process would inherit its predecessor's name.
        const QString program =
            QFileInfo(base + QStringLiteral("/exe")).symLinkTarget();
        if (!program.isEmpty()) {
            p.name = QFileInfo(program).fileName();
        }
        if (p.name.isEmpty()) {
            // No exe in sight. That has two very different reasons, and
            // telling them apart matters: kernel threads genuinely have
            // none, foreign processes merely refuse to show it. cmdline
            // separates the two - only kernel threads lack it. (The
            // first attempt used whether exe exists. That produced
            // "[ModemManager]", because exists() follows the symlink and
            // says no without read permission as well.)
            QFile cmdline(base + QStringLiteral("/cmdline"));
            QByteArray command;
            if (cmdline.open(QIODevice::ReadOnly)) {
                command = cmdline.readAll();
            }
            if (!command.isEmpty()) {
                const QString first =
                    QString::fromLocal8Bit(command.split('\0').constFirst());
                p.name = QFileInfo(first).fileName();
            }
            if (p.name.isEmpty()) {
                QFile comm(base + QStringLiteral("/comm"));
                if (comm.open(QIODevice::ReadOnly)) {
                    p.name = QString::fromLocal8Bit(comm.readAll()).trimmed();
                }
                if (p.name.isEmpty()) {
                    continue;
                }
                // Square brackets for kernel threads, the way ps writes
                // them. Not for those that already carry brackets of
                // their own - "(sd-pam)" must not become "[(sd-pam)]".
                if (command.isEmpty() && !p.name.startsWith(QLatin1Char('('))) {
                    p.name = QLatin1Char('[') + p.name + QLatin1Char(']');
                }
            }
        }

        QFile statm(base + QStringLiteral("/statm"));
        if (statm.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> f = statm.readAll().split(' ');
            if (f.size() > 1) {
                // Field 2 is resident, counted in pages.
                p.memory = f.at(1).toLongLong() * (sysconf(_SC_PAGESIZE) / 1024);
            }
        }

        const uint uid = QFileInfo(base).ownerId();
        p.user = userName(uid);
        p.own = (uid == ownUid);

        // Load from the distance to the previous reading. The reference
        // is the total compute time of all cores, so the values add up
        // to 100 per cent the way the Windows task manager showed them,
        // not to 100 per core the way top does.
        auto previous = m_previous.constFind(pid);
        if (previous != m_previous.constEnd() && now > previous->stamp) {
            const qint64 deltaTicks = ticks - previous->ticks;
            const double seconds = (now - previous->stamp) / 1000.0;
            if (deltaTicks > 0 && seconds > 0) {
                p.cpu = 100.0 * deltaTicks
                        / (seconds * m_ticksPerSecond * m_cores);
            }
        }
        current.insert(pid, Counter{ticks, now});

        list.append(p);
    }

    m_previous = current;

    // Total load from /proc/stat, not as the sum of the individual
    // values: that would miss everything born and gone between two
    // readings.
    QFile total(QStringLiteral("/proc/stat"));
    if (total.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> f = total.readLine().simplified().split(' ');
        if (f.size() > 4) {
            qint64 sum = 0;
            for (int i = 1; i < f.size(); ++i) {
                sum += f.at(i).toLongLong();
            }
            const qint64 idle = f.at(4).toLongLong();
            const qint64 deltaSum = sum - m_totalPrevious;
            const qint64 deltaIdle = idle - m_idlePrevious;
            if (m_totalPrevious > 0 && deltaSum > 0) {
                m_totalCpu = 100.0 * (deltaSum - deltaIdle) / deltaSum;
            }
            m_totalPrevious = sum;
            m_idlePrevious = idle;
        }
    }

    readMemory();

    return list;
}

double ProcessReader::memoryUsage() const
{
    if (m_memoryTotal <= 0) {
        return 0.0;
    }
    return 100.0 * m_memoryUsed / m_memoryTotal;
}

void ProcessReader::readMemory()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    qint64 total = 0;
    qint64 available = 0;
    // readAll and then split, NOT while (!atEnd()) readLine().
    //
    // Files under /proc report a size of 0 - they only come into being
    // as they are read. QFile::atEnd() answers from exactly that size
    // and says yes before the first read. The loop ran zero times, the
    // memory meter sat at 0 per cent, and it looked like an arithmetic
    // bug. readAll() keeps reading in blocks until nothing follows and
    // returns the full 1671 bytes.
    const QList<QByteArray> lines = file.readAll().split('\n');
    for (const QByteArray &line : lines) {
        // MemAvailable, not MemFree: what the cache holds is not in use,
        // it is available on demand. Computed from MemFree, a healthy
        // system reported 95 per cent in use.
        if (line.startsWith("MemTotal:")) {
            total = line.split(':').at(1).simplified().split(' ').at(0).toLongLong();
        } else if (line.startsWith("MemAvailable:")) {
            available =
                line.split(':').at(1).simplified().split(' ').at(0).toLongLong();
            break;
        }
    }
    if (total <= 0) {
        return;
    }
    m_memoryTotal = total;
    m_memoryUsed = total - available;
}
