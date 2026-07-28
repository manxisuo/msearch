#include "search/SearchEngine.h"

#include <QRegExp>

SearchEngine::SearchEngine(IndexDatabase *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

void SearchEngine::cancel()
{
    ++m_token;
}

bool SearchEngine::hasWildcard(const QString &query)
{
    return query.contains(QLatin1Char('*')) || query.contains(QLatin1Char('?'));
}

void SearchEngine::search(const QString &query, bool caseSensitive, int filter, int maxResults)
{
    const quint64 token = ++m_token;
    const QString trimmed = query.trimmed();
    const EntryFilter entryFilter = EntryFilter(filter);
    const int limit = qMax(1, maxResults);

    if (trimmed.isEmpty()) {
        emit resultsReady(QVector<FileEntry>(), trimmed, false);
        emit searchFinished(trimmed, 0, false);
        return;
    }

    const QVector<FileEntry> snapshot = m_db->snapshot();
    QVector<FileEntry> hits;
    hits.reserve(qMin(limit, 1024));

    bool truncated = false;

    for (const FileEntry &e : snapshot) {
        if (token != m_token.load())
            return;

        if (entryFilter == EntryFilter::FilesOnly && e.isDir)
            continue;
        if (entryFilter == EntryFilter::DirsOnly && !e.isDir)
            continue;

        if (!matchName(e.name, trimmed, caseSensitive))
            continue;

        if (hits.size() >= limit) {
            truncated = true;
            break;
        }
        hits.append(e);
    }

    if (token != m_token.load())
        return;

    emit resultsReady(hits, trimmed, truncated);
    emit searchFinished(trimmed, hits.size(), truncated);
}

bool SearchEngine::matchName(const QString &name, const QString &query, bool caseSensitive) const
{
    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

    if (hasWildcard(query)) {
        QRegExp rx(query, cs, QRegExp::Wildcard);
        return rx.exactMatch(name);
    }

    if (caseSensitive)
        return name.contains(query);
    return name.toLower().contains(query.toLower());
}
