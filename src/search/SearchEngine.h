#pragma once

#include "index/FileEntry.h"
#include "index/IndexDatabase.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

enum class EntryFilter {
    All,
    FilesOnly,
    DirsOnly
};

class SearchEngine : public QObject
{
    Q_OBJECT
public:
    explicit SearchEngine(IndexDatabase *db, QObject *parent = nullptr);

public slots:
    void search(const QString &query, bool caseSensitive, int filter, int maxResults);
    void cancel();

signals:
    void resultsReady(const QVector<FileEntry> &results, const QString &query, bool truncated);
    void searchFinished(const QString &query, int matchCount, bool truncated);

private:
    bool matchName(const QString &name, const QString &query, bool caseSensitive) const;
    static bool hasWildcard(const QString &query);

    IndexDatabase *m_db;
    std::atomic<quint64> m_token{0};
};
