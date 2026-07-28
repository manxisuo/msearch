#include "index/FsWatcher.h"
#include "index/MountPolicy.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QRegExp>
#include <QTimer>

FsWatcher::FsWatcher(IndexDatabase *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounce(new QTimer(this))
{
    m_excludeNames = QStringList()
        << QStringLiteral(".git")
        << QStringLiteral(".svn")
        << QStringLiteral(".hg")
        << QStringLiteral("node_modules")
        << QStringLiteral("__pycache__")
        << QStringLiteral(".cache")
        << QStringLiteral(".ccache");

    m_debounce->setSingleShot(true);
    m_debounce->setInterval(350);

    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FsWatcher::onDirectoryChanged);
    connect(m_debounce, &QTimer::timeout, this, &FsWatcher::processQueue);
}

void FsWatcher::setExcludePatterns(const QStringList &patterns)
{
    m_excludePatterns = patterns;
}

void FsWatcher::setSkipHidden(bool on)
{
    m_skipHidden = on;
}

void FsWatcher::setFollowSymlinks(bool on)
{
    m_followSymlinks = on;
}

void FsWatcher::setSkipNetworkMounts(bool on)
{
    m_skipNetwork = on;
}

void FsWatcher::setSkipReadOnlyMounts(bool on)
{
    m_skipReadOnly = on;
}

void FsWatcher::setMaxWatches(int n)
{
    m_maxWatches = qMax(64, n);
}

void FsWatcher::stop()
{
    m_debounce->stop();
    m_pending.clear();
    const QStringList dirs = m_watcher->directories();
    if (!dirs.isEmpty())
        m_watcher->removePaths(dirs);
}

bool FsWatcher::addWatch(const QString &dirPath)
{
    if (m_watcher->directories().contains(dirPath))
        return true;
    if (m_watcher->directories().size() >= m_maxWatches) {
        if (!m_watchLimited) {
            m_watchLimited = true;
            emit statusMessage(tr("监控目录数已达上限（%1），部分子目录依赖父目录变更触发刷新")
                                   .arg(m_maxWatches));
        }
        return false;
    }
    return m_watcher->addPath(dirPath);
}

void FsWatcher::rebuildWatches()
{
    stop();
    m_watchLimited = false;

    const QStringList roots = m_db->includePaths();
    for (const QString &root : roots)
        addWatch(root);

    // Prefer watching directories from index (inotify on Linux via QFileSystemWatcher)
    const QStringList dirs = m_db->directoryPaths(m_maxWatches);
    int added = 0;
    for (const QString &dir : dirs) {
        if (addWatch(dir))
            ++added;
        else
            break;
    }

    emit statusMessage(tr("文件监控已启动：%1 个目录").arg(m_watcher->directories().size()));
    Q_UNUSED(added);
}

void FsWatcher::onDirectoryChanged(const QString &path)
{
    m_pending.insert(path);
    m_debounce->start();
}

bool FsWatcher::shouldSkipName(const QString &name) const
{
    if (m_excludeNames.contains(name))
        return true;
    if (m_skipHidden && name.startsWith(QLatin1Char('.')))
        return true;
    return false;
}

bool FsWatcher::shouldSkipPath(const QString &absolutePath) const
{
    if (absolutePath.startsWith(QLatin1String("/proc"))
        || absolutePath.startsWith(QLatin1String("/sys"))
        || absolutePath.startsWith(QLatin1String("/dev"))
        || absolutePath.startsWith(QLatin1String("/run"))) {
        return true;
    }

    MountPolicy::Options mop;
    mop.skipNetwork = m_skipNetwork;
    mop.skipReadOnly = m_skipReadOnly;
    if (MountPolicy::shouldSkipPath(absolutePath, mop))
        return true;

    for (const QString &pattern : m_excludePatterns) {
        const QString trimmed = pattern.trimmed();
        if (trimmed.isEmpty())
            continue;
        QRegExp rx(trimmed, Qt::CaseSensitive, QRegExp::Wildcard);
        if (rx.exactMatch(absolutePath) || rx.exactMatch(QFileInfo(absolutePath).fileName()))
            return true;
    }
    return false;
}

void FsWatcher::rescanDirectory(const QString &dirPath)
{
    QFileInfo dirInfo(dirPath);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        m_db->removeUnderPrefix(dirPath);
        m_watcher->removePath(dirPath);
        return;
    }

    // Capture old direct children that were directories (to detect removals)
    QSet<QString> oldChildDirs;
    {
        const QVector<FileEntry> snap = m_db->snapshot();
        for (const FileEntry &e : snap) {
            if (e.path == dirPath && e.isDir)
                oldChildDirs.insert(e.fullPath());
        }
    }

    m_db->removeDirectChildren(dirPath);

    // Refresh the directory node itself
    FileEntry self;
    self.name = dirInfo.fileName().isEmpty() ? dirPath : dirInfo.fileName();
    self.path = dirInfo.absolutePath();
    self.size = 0;
    self.mtime = dirInfo.lastModified().toSecsSinceEpoch();
    self.isDir = true;
    m_db->upsert(self);

    QDir::Filters filters = QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::System;
    if (!m_skipHidden)
        filters |= QDir::Hidden;

    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Name);
    QSet<QString> newChildDirs;

    for (const QFileInfo &info : entries) {
        const QString name = info.fileName();
        if (shouldSkipName(name))
            continue;

        const QString abs = info.absoluteFilePath();
        if (shouldSkipPath(abs))
            continue;

        const bool isLink = info.isSymLink();
        if (isLink && !m_followSymlinks) {
            FileEntry entry;
            entry.name = name;
            entry.path = info.absolutePath();
            entry.size = info.isDir() ? 0 : info.size();
            entry.mtime = info.lastModified().toSecsSinceEpoch();
            entry.isDir = info.isDir();
            m_db->add(entry);
            continue;
        }

        FileEntry entry;
        entry.name = name;
        entry.path = info.absolutePath();
        entry.size = info.isDir() ? 0 : info.size();
        entry.mtime = info.lastModified().toSecsSinceEpoch();
        entry.isDir = info.isDir();
        m_db->add(entry);

        if (info.isDir()) {
            newChildDirs.insert(abs);
            addWatch(abs);
        }
    }

    // Removed subdirectories: drop their whole subtree
    for (const QString &oldDir : oldChildDirs) {
        if (!newChildDirs.contains(oldDir)) {
            m_db->removeUnderPrefix(oldDir);
            m_watcher->removePath(oldDir);
        }
    }
}

void FsWatcher::processQueue()
{
    if (m_pending.isEmpty())
        return;

    const QSet<QString> batch = m_pending;
    m_pending.clear();

    for (const QString &path : batch)
        rescanDirectory(path);

    emit indexUpdated();
}
