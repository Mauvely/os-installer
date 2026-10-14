#include "core/target.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace target {
namespace {

// open(2) with the mode applied explicitly: QFile cannot create a file 0600
// without a window in which it exists with the umask's mode, and `enrol` holds
// a secret.
bool writeFile(const QString &path, const QByteArray &content, mode_t mode, QString *error) {
    const QByteArray p = QFile::encodeName(path);
    const int fd = ::open(p.constData(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, mode);
    if (fd < 0) {
        if (error) *error = QStringLiteral("Could not create %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    bool ok = ::fchmod(fd, mode) == 0;
    qint64 done = 0;
    while (ok && done < content.size()) {
        const ssize_t n = ::write(fd, content.constData() + done, content.size() - done);
        if (n < 0 && errno == EINTR) continue;
        ok = n > 0;
        if (ok) done += n;
    }
    ok = ok && ::fsync(fd) == 0;
    const int err = errno;
    ::close(fd);
    if (!ok && error)
        *error = QStringLiteral("Could not write %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(err)));
    return ok;
}

void syncDir(const QString &path) {
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
}

}  // namespace

QString findDeployment(const QString &root, QString *error) {
    QStringList found;
    const QDir deploy(root + QStringLiteral("/ostree/deploy"));
    for (const QString &os : deploy.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QDir d(deploy.filePath(os) + QStringLiteral("/deploy"));
        // `<checksum>.<serial>` directories; the `.origin` files beside them are files.
        for (const QString &dep : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            if (QFileInfo(d.filePath(dep) + QStringLiteral("/etc")).isDir()) found << d.filePath(dep);
    }
    if (found.size() == 1) return found.first();
    if (error)
        *error = found.isEmpty() ? QStringLiteral("The new system has no ostree deployment.")
                                 : QStringLiteral("The new system has more than one ostree deployment.");
    return {};
}

bool writeEnrolment(const QString &deployment, const QString &server, const QString &token,
                    QString *error) {
    const QString dir = deployment + QStringLiteral("/etc/mauvely");
    const QByteArray d = QFile::encodeName(dir);
    if (::mkdir(d.constData(), 0755) != 0 && errno != EEXIST) {
        if (error) *error = QStringLiteral("Could not create %1: %2").arg(dir, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    ::chmod(d.constData(), 0755);
    if (!writeFile(dir + QStringLiteral("/server"), server.toUtf8() + '\n', 0644, error)) return false;
    if (!writeFile(dir + QStringLiteral("/enrol"), token.toUtf8() + '\n', 0600, error)) return false;
    syncDir(dir);
    syncDir(deployment + QStringLiteral("/etc"));
    return true;
}

QString selinuxType(const QString &deployment) {
    QFile f(deployment + QStringLiteral("/etc/selinux/config"));
    QString type;
    if (f.open(QIODevice::ReadOnly)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.startsWith(QLatin1String("SELINUXTYPE=")))
                type = line.mid(12).trimmed().remove(QLatin1Char('"'));
        }
    }
    return type.isEmpty() ? QStringLiteral("targeted") : type;
}

QStringList setfilesArgs(const QString &deployment) {
    return {QStringLiteral("-F"),  // force: set the whole context, user and role included
            QStringLiteral("-r"), deployment,
            deployment + QStringLiteral("/etc/selinux/") + selinuxType(deployment) +
                QStringLiteral("/contexts/files/file_contexts"),
            deployment + QStringLiteral("/etc/mauvely")};
}

}  // namespace target
