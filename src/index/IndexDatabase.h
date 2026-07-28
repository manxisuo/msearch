#pragma once

#include "index/FileEntry.h"

#include <QMutex>
#include <QStringList>
#include <QVector>

class IndexDatabase
{
public:
    void clear();
    void reserve(int n);
    void add(const FileEntry &entry);
    void addBatch(const QVector<FileEntry> &batch);

    // Incremental updates
    void upsert(const FileEntry &entry);
    int removeByFullPath(const QString &fullPath);
    int removeUnderPrefix(const QString &prefix);
    int removeDirectChildren(const QString &parentPath);

    int count() const;
    QVector<FileEntry> snapshot() const;
    QStringList directoryPaths(int maxCount = -1) const;

    bool saveToFile(const QString &filePath) const;
    bool loadFromFile(const QString &filePath);
    bool isLoaded() const;

    QStringList includePaths() const;
    void setIncludePaths(const QStringList &paths);

    bool isDirty() const;
    void markClean();

private:
    static QString entryFullPath(const FileEntry &e);

    mutable QMutex m_mutex;
    QVector<FileEntry> m_entries;
    QStringList m_includePaths;
    bool m_dirty = false;
    bool m_loaded = false;
};
