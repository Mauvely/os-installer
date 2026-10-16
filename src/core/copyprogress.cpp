#include "core/copyprogress.h"

#include <QRegularExpression>

namespace copyprogress {
namespace {

constexpr qint64 kSector = 512;       // /sys/block/*/stat counts in these, always
constexpr double kCeiling = 0.99;     // short of full until the step really ends
constexpr qint64 kMinReported = 100'000'000;  // the partitioning and LUKS header write less

// glib's g_format_size, which is what bootc prints with: kB, MB, GB, TB for
// powers of 1000 and KiB, MiB, GiB, TiB for 1024, or "byte(s)" under a kB.
double unitBytes(const QString &unit) {
    const QString u = unit.toLower();
    if (u == QLatin1String("b") || u == QLatin1String("byte") || u == QLatin1String("bytes")) return 1;
    static const struct { const char *name; double bytes; } units[] = {
        {"kb", 1e3},  {"mb", 1e6},  {"gb", 1e9},  {"tb", 1e12},
        {"kib", 1024.0}, {"mib", 1048576.0}, {"gib", 1073741824.0}, {"tib", 1099511627776.0},
    };
    for (const auto &e : units)
        if (u == QLatin1String(e.name)) return e.bytes;
    return 0;
}

}  // namespace

qint64 totalFromLine(const QString &line) {
    static const QRegularExpression re(
        QStringLiteral(R"(layers needed: \d+ \(([0-9]+(?:\.[0-9]+)?) ([A-Za-z]+)\))"));
    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch()) return 0;
    const double unit = unitBytes(m.captured(2));
    return unit > 0 ? qRound64(m.captured(1).toDouble() * unit) : 0;
}

qint64 sectorsWritten(const QByteArray &stat) {
    const QList<QByteArray> fields = stat.simplified().split(' ');
    if (fields.size() < 7) return -1;
    bool ok = false;
    const qint64 sectors = fields.at(6).toLongLong(&ok);
    return ok && sectors >= 0 ? sectors : -1;
}

QString sysfsName(const QString &devicePath) {
    static const QString dev = QStringLiteral("/dev/");
    if (!devicePath.startsWith(dev) || devicePath.size() == dev.size()) return {};
    return devicePath.mid(dev.size()).replace(QLatin1Char('/'), QLatin1Char('!'));
}

void Meter::sample(qint64 sectors) {
    if (sectors < 0) return;
    if (!m_haveBase) {
        m_base = sectors;
        m_haveBase = true;
        return;
    }
    m_written = qMax(m_written, (sectors - m_base) * kSector);
}

double Meter::fraction() const {
    if (!m_haveBase || m_total <= 0) return -1;
    return qMin(static_cast<double>(m_written) / static_cast<double>(m_total), kCeiling);
}

QString Meter::text() const {
    if (!m_haveBase) return {};
    if (m_total > 0) {
        const qint64 done = qMin(m_written, m_total);
        if (m_total >= 1'000'000'000)
            return QStringLiteral("%1 of %2 GB").arg(done / 1e9, 0, 'f', 1).arg(m_total / 1e9, 0, 'f', 1);
        return QStringLiteral("%1 of %2 MB").arg(qRound64(done / 1e6)).arg(qRound64(m_total / 1e6));
    }
    if (m_written < kMinReported) return {};
    if (m_written >= 1'000'000'000) return QStringLiteral("%1 GB written").arg(m_written / 1e9, 0, 'f', 1);
    return QStringLiteral("%1 MB written").arg(qRound64(m_written / 1e6));
}

}  // namespace copyprogress
