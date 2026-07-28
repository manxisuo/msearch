#pragma once

#include "index/IndexDatabase.h"

#include <QObject>
#include <QStringList>
#include <atomic>

class Indexer : public QObject
{
    Q_OBJECT
public:
    explicit Indexer(IndexDatabase *db, QObject *parent = nullptr);

    void setIncludePaths(const QStringList &paths);
    void setExcludeNames(const QStringList &names);
    void setExcludePatterns(const QStringList &patterns);
    void setSkipHidden(bool on);
    void setFollowSymlinks(bool on);
    void setSkipNetworkMounts(bool on);
    void setSkipReadOnlyMounts(bool on);
    void setClearBeforeIndex(bool on) { m_clearBefore = on; }
    QStringList includePaths() const { return m_includePaths; }

    bool isRunning() const { return m_running.load(); }

public slots:
    void start();
    void cancel();
    void loadFromFile(const QString &filePath);

signals:
    void progress(qint64 filesFound, const QString &currentPath, double filesPerSec);
    void finished(bool cancelled, qint64 totalFiles);
    void loadFinished(bool ok, int count, const QString &error);
    void error(const QString &message);

private:
    void walkDirectory(const QString &rootPath);
    bool shouldSkipName(const QString &name) const;
    bool shouldSkipPath(const QString &absolutePath) const;

    IndexDatabase *m_db;
    QStringList m_includePaths;
    QStringList m_excludeNames;
    QStringList m_excludePatterns;
    bool m_skipHidden = false;
    bool m_followSymlinks = false;
    bool m_skipNetwork = true;
    bool m_skipReadOnly = false;
    bool m_clearBefore = true;
    std::atomic<bool> m_cancel{false};
    std::atomic<bool> m_running{false};
    qint64 m_found = 0;
};
