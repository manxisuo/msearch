#pragma once

#include "index/IndexDatabase.h"

#include <QObject>
#include <QStringList>
#include <QSet>

class QFileSystemWatcher;
class QTimer;

class FsWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FsWatcher(IndexDatabase *db, QObject *parent = nullptr);

    void setExcludePatterns(const QStringList &patterns);
    void setSkipHidden(bool on);
    void setFollowSymlinks(bool on);
    void setSkipNetworkMounts(bool on);
    void setSkipReadOnlyMounts(bool on);
    void setMaxWatches(int n);

    void rebuildWatches();
    void stop();

signals:
    void indexUpdated();
    void statusMessage(const QString &text);

private slots:
    void onDirectoryChanged(const QString &path);
    void processQueue();

private:
    bool shouldSkipName(const QString &name) const;
    bool shouldSkipPath(const QString &absolutePath) const;
    void rescanDirectory(const QString &dirPath);
    bool addWatch(const QString &dirPath);

    IndexDatabase *m_db = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
    QSet<QString> m_pending;
    QStringList m_excludePatterns;
    QStringList m_excludeNames;
    bool m_skipHidden = false;
    bool m_followSymlinks = false;
    bool m_skipNetwork = true;
    bool m_skipReadOnly = false;
    int m_maxWatches = 8192;
    bool m_watchLimited = false;
};
