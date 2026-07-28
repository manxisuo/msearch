#include "index/IndexDatabase.h"

#include <QDataStream>
#include <QFile>
#include <QMutexLocker>

static const quint32 kMagic = 0x4D534442; // 'MSDB'
static const quint32 kVersion = 1;

void IndexDatabase::clear()
{
    QMutexLocker lock(&m_mutex);
    m_entries.clear();
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
}

void IndexDatabase::addBatch(const QVector<FileEntry> &batch)
{
    QMutexLocker lock(&m_mutex);
    m_entries += batch;
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

    QVector<FileEntry> entries;
    entries.reserve(int(count));
    for (quint32 i = 0; i < count; ++i) {
        FileEntry e;
        in >> e.name >> e.path >> e.size >> e.mtime >> e.isDir;
        if (in.status() != QDataStream::Ok)
            return false;
        entries.append(e);
    }

    QMutexLocker lock(&m_mutex);
    m_includePaths = includePaths;
    m_entries = entries;
    return true;
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
}
