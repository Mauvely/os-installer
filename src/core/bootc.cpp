#include "core/bootc.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace bootc {

QStringList installArgs(const QString &payloadRef, const QString &disk) {
    return {QStringLiteral("install"),
            QStringLiteral("to-disk"),
            QStringLiteral("--source-imgref"),
            QStringLiteral("containers-storage:") + payloadRef,
            QStringLiteral("--wipe"),
            QStringLiteral("--block-setup"),
            QStringLiteral("tpm2-luks"),
            QStringLiteral("--filesystem"),  // the base image names no default (P1, S1 trap 4)
            QStringLiteral("xfs"),
            QStringLiteral("--target-imgref"),
            targetImgref(),
            QStringLiteral("--skip-fetch-check"),
            disk};
}

Outcome classify(int exitCode, const QString &output) {
    if (exitCode == 0) return Outcome::Succeeded;
    if (exitCode < 0) return Outcome::Failed;  // killed, or never started: bootc did not finish

    // The *last* line must be the known failure. The same words earlier in the
    // output, followed by something else, are not this case.
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString l = it->trimmed();
        if (l.isEmpty()) continue;
        return l.startsWith(QLatin1String("error:")) &&
                       l.contains(QLatin1String("Task Closing root LUKS device failed"))
                   ? Outcome::LeakedMount
                   : Outcome::Failed;
    }
    return Outcome::Failed;
}

QList<QStringList> leakedMountWorkaround() {
    return {{QStringLiteral("umount"), QStringLiteral("/run/bootc/storage")},
            {QStringLiteral("cryptsetup"), QStringLiteral("close"), QStringLiteral("root")}};
}

QString payloadFromPodmanImages(const QByteArray &json, QString *error) {
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        if (error) *error = QStringLiteral("podman printed something that is not its JSON");
        return {};
    }
    // One entry per image; an image with several tags lists them all under
    // `Names`, so images are counted, not names. Untagged layers have none.
    QStringList refs;
    for (const QJsonValue &v : doc.array()) {
        const QJsonArray names = v.toObject().value(QLatin1String("Names")).toArray();
        const QString ref = names.isEmpty() ? QString() : names.first().toString();
        if (!ref.isEmpty()) refs << ref;
    }
    if (refs.size() == 1) return refs.first();
    if (error)
        *error = refs.isEmpty() ? QStringLiteral("The install medium carries no OS image.")
                                : QStringLiteral("The install medium carries more than one image.");
    return {};
}

}  // namespace bootc
