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

    int count() const;
    QVector<FileEntry> snapshot() const;

    bool saveToFile(const QString &filePath) const;
    bool loadFromFile(const QString &filePath);

    QStringList includePaths() const;
    void setIncludePaths(const QStringList &paths);

private:
    mutable QMutex m_mutex;
    QVector<FileEntry> m_entries;
    QStringList m_includePaths;
};
