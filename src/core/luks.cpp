#include "core/luks.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace luks {
namespace {

void collect(const QJsonObject &o, QStringList *out) {
    if (o.value(QLatin1String("fstype")).toString() == QLatin1String("crypto_LUKS"))
        *out << o.value(QLatin1String("path")).toString();
    for (const QJsonValue &c : o.value(QLatin1String("children")).toArray())
        collect(c.toObject(), out);
}

}  // namespace

QStringList lsblkArgs(const QString &disk) {
    return {QStringLiteral("--json"), QStringLiteral("--tree"), QStringLiteral("--output"),
            QStringLiteral("NAME,PATH,TYPE,FSTYPE"), disk};
}

QString partition(const QByteArray &lsblkJson, QString *error) {
    const QJsonArray devices =
        QJsonDocument::fromJson(lsblkJson).object().value(QLatin1String("blockdevices")).toArray();
    QStringList found;
    for (const QJsonValue &v : devices) collect(v.toObject(), &found);
    if (found.size() == 1 && !found.first().isEmpty()) return found.first();
    if (error)
        *error = found.isEmpty() ? QStringLiteral("The new disk has no encrypted partition.")
                                 : QStringLiteral("The new disk has more than one encrypted partition.");
    return {};
}

QStringList addRecoveryKeyArgs(const QString &device) {
    return {QStringLiteral("--unlock-tpm2-device=auto"), QStringLiteral("--recovery-key"), device};
}

QString recoveryKey(const QString &output) {
    static const QRegularExpression re(
        QStringLiteral("(?<![a-z-])[cbdefghijklnrtuv]{8}(?:-[cbdefghijklnrtuv]{8}){7}(?![a-z-])"));
    QSet<QString> keys;
    QString first;
    for (auto it = re.globalMatch(output); it.hasNext();) {
        const QString k = it.next().captured(0);
        if (keys.isEmpty()) first = k;
        keys.insert(k);
    }
    return keys.size() == 1 ? first : QString();
}

}  // namespace luks
