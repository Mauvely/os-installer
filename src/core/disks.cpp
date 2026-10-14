#include "core/disks.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace disks {
namespace {

// lsblk has printed booleans and sizes both as JSON values and as strings
// across util-linux releases ("rm": true, "rm": "1"; "size": 1024, "size":
// "1024"), so every field is read through these rather than trusted to a type.
bool flag(const QJsonValue &v) {
    if (v.isBool()) return v.toBool();
    if (v.isDouble()) return v.toInt() != 0;
    const QString s = v.toString().trimmed();
    return s == QLatin1String("1") || s == QLatin1String("true");
}

qint64 bytes(const QJsonValue &v) {
    if (v.isDouble()) return static_cast<qint64>(v.toDouble());
    return v.toString().trimmed().toLongLong();
}

QString text(const QJsonValue &v) { return v.isString() ? v.toString().trimmed() : QString(); }

// `mountpoints` (an array, null when unmounted) since util-linux 2.37,
// `mountpoint` (one string) before it.
bool mountedHere(const QJsonObject &o) {
    for (const QJsonValue &m : o.value(QLatin1String("mountpoints")).toArray())
        if (!text(m).isEmpty()) return true;
    return !text(o.value(QLatin1String("mountpoint"))).isEmpty();
}

// Depth-first over a device and everything below it: partitions, and the
// dm, md or loop devices stacked on those.
template <typename Pred>
bool anyInTree(const QJsonObject &o, Pred pred) {
    if (pred(o)) return true;
    for (const QJsonValue &c : o.value(QLatin1String("children")).toArray())
        if (anyInTree(c.toObject(), pred)) return true;
    return false;
}

QString transportName(const QString &tran) {
    const QString t = tran.toLower();
    if (t == QLatin1String("nvme")) return QStringLiteral("NVMe");
    if (t == QLatin1String("usb")) return QStringLiteral("USB");
    if (t == QLatin1String("mmc")) return QStringLiteral("MMC");
    return tran.toUpper();  // SATA, ATA, SAS, SCSI, …
}

}  // namespace

QString Disk::title() const { return model.isEmpty() ? name : model; }

QString Disk::detail() const {
    QStringList parts{humanSize(sizeBytes)};
    if (!transport.isEmpty()) parts << transportName(transport);
    parts << path;
    return parts.join(QStringLiteral(" · "));
}

QStringList lsblkArgs() {
    return {QStringLiteral("--json"), QStringLiteral("--bytes"), QStringLiteral("--tree"),
            QStringLiteral("--output"),
            QStringLiteral("NAME,PATH,TYPE,SIZE,MODEL,SERIAL,TRAN,RM,RO,MOUNTPOINTS,LABEL")};
}

QList<Disk> parse(const QByteArray &json, const QString &liveSource,
                  const QString &liveLabel, QString *error) {
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    const QJsonValue devices = doc.object().value(QLatin1String("blockdevices"));
    if (pe.error != QJsonParseError::NoError || !devices.isArray()) {
        if (error) *error = QStringLiteral("lsblk printed something that is not its JSON");
        return {};
    }

    QList<Disk> out;
    for (const QJsonValue &v : devices.toArray()) {
        const QJsonObject o = v.toObject();
        if (text(o.value(QLatin1String("type"))) != QLatin1String("disk")) continue;

        Disk d;
        d.name = text(o.value(QLatin1String("name")));
        d.path = text(o.value(QLatin1String("path")));
        if (d.path.isEmpty()) d.path = QStringLiteral("/dev/") + d.name;
        d.model = text(o.value(QLatin1String("model")));
        d.serial = text(o.value(QLatin1String("serial")));
        d.transport = text(o.value(QLatin1String("tran")));
        d.sizeBytes = bytes(o.value(QLatin1String("size")));
        d.removable = flag(o.value(QLatin1String("rm")));

        const bool isLive = anyInTree(o, [&](const QJsonObject &n) {
            if (!liveSource.isEmpty() && text(n.value(QLatin1String("path"))) == liveSource)
                return true;
            return !liveLabel.isEmpty() && text(n.value(QLatin1String("label"))) == liveLabel;
        });

        if (d.name.startsWith(QLatin1String("zram")))
            d.exclusion = Exclusion::Virtual;
        else if (isLive)
            d.exclusion = Exclusion::LiveMedium;
        else if (anyInTree(o, mountedHere))
            d.exclusion = Exclusion::Mounted;
        else if (flag(o.value(QLatin1String("ro"))))
            d.exclusion = Exclusion::ReadOnly;
        else if (d.sizeBytes <= 0)
            d.exclusion = Exclusion::Empty;
        out << d;
    }
    return out;
}

QList<Disk> eligible(const QList<Disk> &all) {
    QList<Disk> out;
    for (const Disk &d : all)
        if (d.eligible()) out << d;
    return out;
}

QString humanSize(qint64 b) {
    const double gb = static_cast<double>(b) / 1e9;
    if (gb >= 1000.0) {
        const double tb = gb / 1000.0;
        return QString::number(tb, 'f', tb < 10.0 ? 1 : 0).remove(QStringLiteral(".0")) +
               QStringLiteral(" TB");
    }
    if (gb >= 10.0) return QString::number(qRound(gb)) + QStringLiteral(" GB");
    return QString::number(gb, 'f', 1).remove(QStringLiteral(".0")) + QStringLiteral(" GB");
}

}  // namespace disks
