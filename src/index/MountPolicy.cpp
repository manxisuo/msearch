#include "index/MountPolicy.h"

#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QTextStream>

#ifdef Q_OS_LINUX
#  include <sys/statvfs.h>
#endif

namespace MountPolicy {

static QString normalizePath(const QString &path)
{
    QString p = QFileInfo(path).absoluteFilePath();
    while (p.size() > 1 && p.endsWith(QLatin1Char('/')))
        p.chop(1);
    return p;
}

static bool isNetworkFsType(const QString &type)
{
    const QString t = type.toLower();
    return t == QLatin1String("nfs")
        || t.startsWith(QLatin1String("nfs"))
        || t == QLatin1String("cifs")
        || t == QLatin1String("smb")
        || t == QLatin1String("smb3")
        || t == QLatin1String("smbfs")
        || t.contains(QLatin1String("sshfs"))
        || t == QLatin1String("fuse.sshfs")
        || t == QLatin1String("fuse.gvfsd-fuse")
        || t == QLatin1String("afs")
        || t == QLatin1String("9p")
        || t == QLatin1String("ceph")
        || t == QLatin1String("glusterfs");
}

#ifdef Q_OS_LINUX
static QString fsTypeFromProcMounts(const QString &absolutePath)
{
    const QString target = normalizePath(absolutePath);
    QFile file(QStringLiteral("/proc/mounts"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QString bestMount;
    QString bestType;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QStringList parts = line.split(QLatin1Char(' '), QString::SkipEmptyParts);
        if (parts.size() < 3)
            continue;
        QString mount = parts.at(1);
        mount.replace(QStringLiteral("\\040"), QStringLiteral(" "));
        const QString type = parts.at(2);
        if (target == mount || target.startsWith(mount.endsWith(QLatin1Char('/')) ? mount : mount + QLatin1Char('/'))
            || (mount == QLatin1String("/") && target.startsWith(QLatin1Char('/')))) {
            if (mount.size() >= bestMount.size()) {
                bestMount = mount;
                bestType = type;
            }
        }
    }
    return bestType;
}
#endif

bool isNetworkPath(const QString &absolutePath)
{
#ifdef Q_OS_LINUX
    const QString type = fsTypeFromProcMounts(absolutePath);
    if (!type.isEmpty() && isNetworkFsType(type))
        return true;
#endif
    // Fallback: QStorageInfo device/fileSystemType when available
    const QStorageInfo info(absolutePath);
    if (info.isValid()) {
        const QString type = QString::fromUtf8(info.fileSystemType()).toLower();
        if (isNetworkFsType(type))
            return true;
    }
    return false;
}

bool isReadOnlyPath(const QString &absolutePath)
{
#ifdef Q_OS_LINUX
    struct statvfs st;
    if (statvfs(absolutePath.toLocal8Bit().constData(), &st) == 0) {
        if (st.f_flag & ST_RDONLY)
            return true;
    }
#endif
    const QStorageInfo info(absolutePath);
    if (info.isValid() && info.isReadOnly())
        return true;
    return false;
}

bool shouldSkipPath(const QString &absolutePath, const Options &opt)
{
    if (opt.skipNetwork && isNetworkPath(absolutePath))
        return true;
    if (opt.skipReadOnly && isReadOnlyPath(absolutePath))
        return true;
    return false;
}

} // namespace MountPolicy
