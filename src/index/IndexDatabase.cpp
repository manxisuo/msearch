#include "index/IndexDatabase.h"

#include <QDataStream>
#include <QFile>
#include <QMutexLocker>

static const quint32 kMagic = 0x4D534442; // 'MSDB'
static const quint32 kVersion = 1;

QString IndexDatabase::entryFullPath(const FileEntry &e)
{
    return e.fullPath();
}

void IndexDatabase::clear()
{
    QMutexLocker lock(&m_mutex);
    m_entries.clear();
    m_dirty = true;
    m_loaded = true;
}

void IndexDatabase::reserve(int n)
{
    QMutexLocker lock(&m_mutex);
    m_entries.reserve(n);
}

void IndexDatabase::add(const FileEntry &entry)
{
    QMutexLocker lock(&m_mutex);
    m_entries.append(entry);
    m_dirty = true;
}

void IndexDatabase::addBatch(const QVector<FileEntry> &batch)
{
    QMutexLocker lock(&m_mutex);
    m_entries += batch;
    m_dirty = true;
}

void IndexDatabase::upsert(const FileEntry &entry)
{
    QMutexLocker lock(&m_mutex);
    const QString full = entryFullPath(entry);
    for (int i = 0; i < m_entries.size(); ++i) {
        if (entryFullPath(m_entries.at(i)) == full) {
            m_entries[i] = entry;
            m_dirty = true;
            return;
        }
    }
    m_entries.append(entry);
    m_dirty = true;
}

int IndexDatabase::removeByFullPath(const QString &fullPath)
{
    QMutexLocker lock(&m_mutex);
    int removed = 0;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (entryFullPath(m_entries.at(i)) == fullPath) {
            m_entries.removeAt(i);
            ++removed;
        }
    }
    if (removed)
        m_dirty = true;
    return removed;
}

int IndexDatabase::removeUnderPrefix(const QString &prefix)
{
    QMutexLocker lock(&m_mutex);
    const QString withSlash = prefix.endsWith(QLatin1Char('/')) ? prefix : prefix + QLatin1Char('/');
    int removed = 0;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        const QString full = entryFullPath(m_entries.at(i));
        if (full == prefix || full.startsWith(withSlash)) {
            m_entries.removeAt(i);
            ++removed;
        }
    }
    if (removed)
        m_dirty = true;
    return removed;
}

int IndexDatabase::removeDirectChildren(const QString &parentPath)
{
    QMutexLocker lock(&m_mutex);
    int removed = 0;
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries.at(i).path == parentPath) {
            m_entries.removeAt(i);
            ++removed;
        }
    }
    if (removed)
        m_dirty = true;
    return removed;
}

int IndexDatabase::count() const
{
    QMutexLocker lock(&m_mutex);
    return m_entries.size();
}

QVector<FileEntry> IndexDatabase::snapshot() const
{
    QMutexLocker lock(&m_mutex);
    return m_entries;
}

QStringList IndexDatabase::directoryPaths(int maxCount) const
{
    QMutexLocker lock(&m_mutex);
    QStringList dirs;
    dirs.reserve(qMin(m_entries.size(), maxCount > 0 ? maxCount : m_entries.size()));
    for (const FileEntry &e : m_entries) {
        if (!e.isDir)
            continue;
        dirs.append(entryFullPath(e));
        if (maxCount > 0 && dirs.size() >= maxCount)
            break;
    }
    return dirs;
}

bool IndexDatabase::saveToFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QMutexLocker lock(&m_mutex);
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_5_12);
    out << kMagic << kVersion;
    out << m_includePaths;
    out << quint32(m_entries.size());
    for (const FileEntry &e : m_entries) {
        out << e.name << e.path << e.size << e.mtime << e.isDir;
    }
    // Trailing marker helps detect truncated files
    out << quint32(0xA5A5A5A5);
    return out.status() == QDataStream::Ok;
}

bool IndexDatabase::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_12);

    quint32 magic = 0;
    quint32 version = 0;
    in >> magic >> version;
    if (magic != kMagic || version != kVersion)
        return false;

    QStringList includePaths;
    quint32 count = 0;
    in >> includePaths >> count;

    // Sanity: reject absurd counts (corruption / wrong file)
    if (count > 200000000u)
        return false;

    QVector<FileEntry> entries;
    entries.reserve(int(qMin(count, quint32(1000000))));
    for (quint32 i = 0; i < count; ++i) {
        FileEntry e;
        in >> e.name >> e.path >> e.size >> e.mtime >> e.isDir;
        if (in.status() != QDataStream::Ok)
            return false;
        if (e.name.isEmpty() && e.path.isEmpty())
            return false;
        entries.append(e);
    }

    // Optional trailing marker (new saves). Old files without it still load if stream ended cleanly.
    if (!in.atEnd()) {
        quint32 marker = 0;
        in >> marker;
        if (in.status() == QDataStream::Ok && marker != 0xA5A5A5A5 && marker != 0)
            return false;
    }

    QMutexLocker lock(&m_mutex);
    m_includePaths = includePaths;
    m_entries = entries;
    m_dirty = false;
    m_loaded = true;
    return true;
}

bool IndexDatabase::isLoaded() const
{
    QMutexLocker lock(&m_mutex);
    return m_loaded;
}

QStringList IndexDatabase::includePaths() const
{
    QMutexLocker lock(&m_mutex);
    return m_includePaths;
}

void IndexDatabase::setIncludePaths(const QStringList &paths)
{
    QMutexLocker lock(&m_mutex);
    m_includePaths = paths;
    m_dirty = true;
}

bool IndexDatabase::isDirty() const
{
    QMutexLocker lock(&m_mutex);
    return m_dirty;
}

void IndexDatabase::markClean()
{
    QMutexLocker lock(&m_mutex);
    m_dirty = false;
}
