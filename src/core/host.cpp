#include "core/host.h"

#include "core/cmdline.h"
#include "core/copyprogress.h"
#include "core/runner.h"

#include <QDir>
#include <QFile>

namespace host {

QString kernelCmdline(const Config &config) {
    QFile f(config.procCmdline);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
}

bool isLiveSystem(const Config &config) {
    return cmdline::hasFlag(kernelCmdline(config), QStringLiteral("rd.live.image"));
}

bool hasTpm2(const Config &config) {
    const QDir dir(config.sysClassTpm);
    for (const QString &tpm : dir.entryList({QStringLiteral("tpm*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile f(dir.filePath(tpm) + QStringLiteral("/tpm_version_major"));
        if (f.open(QIODevice::ReadOnly) && f.readAll().trimmed() == "2") return true;
    }
    return false;
}

qint64 sectorsWritten(const Config &config, const QString &devicePath) {
    const QString name = copyprogress::sysfsName(devicePath);
    if (name.isEmpty()) return -1;
    QFile f(config.sysBlock + QLatin1Char('/') + name + QStringLiteral("/stat"));
    return f.open(QIODevice::ReadOnly) ? copyprogress::sectorsWritten(f.readAll()) : -1;
}

QList<disks::Disk> listDisks(Runner &runner, const Config &config, QString *error) {
    // Empty when nothing is mounted there, which `parse()` handles: the label
    // still marks the medium.
    const RunResult live = runner.run(QStringLiteral("findmnt"),
                                      {QStringLiteral("-n"), QStringLiteral("-o"),
                                       QStringLiteral("SOURCE"), config.liveMountPoint});
    const QString liveSource = live.exitCode == 0 ? live.output.trimmed() : QString();

    const RunResult r = runner.run(QStringLiteral("lsblk"), disks::lsblkArgs());
    if (r.exitCode != 0) {
        if (error) *error = QStringLiteral("lsblk failed: ") + r.output.trimmed();
        return {};
    }
    return disks::parse(r.output.toUtf8(), liveSource, config.liveLabel, error);
}

}  // namespace host
