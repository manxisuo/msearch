#include "search/SearchEngine.h"
#include "search/Pinyin.h"
#include "search/QueryParser.h"

#include <QRegExp>
#include <QRegularExpression>

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

QString SearchEngine::fileExtension(const QString &name)
{
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot == name.size() - 1)
        return QString();
    return name.mid(dot + 1).toLower();
}

bool SearchEngine::matchText(const QString &haystack, const QString &needle, bool caseSensitive) const
{
    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (hasWildcard(needle)) {
        QRegExp rx(needle, cs, QRegExp::Wildcard);
        return rx.exactMatch(haystack);
    }
    if (caseSensitive)
        return haystack.contains(needle);
    return haystack.toLower().contains(needle.toLower());
}

bool SearchEngine::matchEntry(const FileEntry &e, const ParsedQuery &pq, bool caseSensitive, bool pinyinEnabled) const
{
    if (!pq.extensions.isEmpty()) {
        const QString ext = fileExtension(e.name);
        if (!pq.extensions.contains(ext))
            return false;
    }

    if (!e.isDir) {
        if (pq.minSize >= 0 && e.size < pq.minSize)
            return false;
        if (pq.maxSize >= 0 && e.size > pq.maxSize)
            return false;
    } else if (pq.minSize >= 0 || pq.maxSize >= 0) {
        return false;
    }

    if (pq.minMtime >= 0 && e.mtime < pq.minMtime)
        return false;
    if (pq.maxMtime >= 0 && e.mtime > pq.maxMtime)
        return false;

    for (const QueryTerm &term : pq.terms) {
        bool hit = false;
        switch (term.kind) {
        case QueryTerm::Name: {
            if (term.value.contains(QLatin1Char('|')) && !hasWildcard(term.value)) {
                const QStringList alts = term.value.split(QLatin1Char('|'), QString::SkipEmptyParts);
                for (const QString &alt : alts) {
                    if (matchText(e.name, alt.trimmed(), caseSensitive)
                        || (pinyinEnabled && Pinyin::initialsContain(e.name, alt.trimmed(), caseSensitive))) {
                        hit = true;
                        break;
                    }
                }
            } else {
                hit = matchText(e.name, term.value, caseSensitive);
                if (!hit && pinyinEnabled && !hasWildcard(term.value)) {
                    bool asciiNeedle = true;
                    for (const QChar &c : term.value) {
                        if (c.unicode() > 127) {
                            asciiNeedle = false;
                            break;
                        }
                    }
                    if (asciiNeedle)
                        hit = Pinyin::initialsContain(e.name, term.value, caseSensitive);
                }
            }
            break;
        }
        case QueryTerm::Path: {
            const QString full = e.fullPath();
            if (term.value.contains(QLatin1Char('|')) && !hasWildcard(term.value)) {
                const QStringList alts = term.value.split(QLatin1Char('|'), QString::SkipEmptyParts);
                for (const QString &alt : alts) {
                    const QString a = alt.trimmed();
                    if (matchText(e.path, a, caseSensitive) || matchText(full, a, caseSensitive)) {
                        hit = true;
                        break;
                    }
                }
            } else {
                hit = matchText(e.path, term.value, caseSensitive)
                      || matchText(full, term.value, caseSensitive);
            }
            break;
        }
        case QueryTerm::Regex: {
            QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
            if (!caseSensitive)
                opts |= QRegularExpression::CaseInsensitiveOption;
            const QRegularExpression re(term.value, opts);
            if (!re.isValid())
                return false;
            hit = re.match(e.name).hasMatch();
            break;
        }
        case QueryTerm::Ext:
            break;
        }

        if (term.exclude) {
            if (hit)
                return false;
        } else if (!hit) {
            return false;
        }
    }

    return true;
}

void SearchEngine::search(const QString &query, bool caseSensitive, int filter, int maxResults, bool pinyinEnabled)
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

    const ParsedQuery pq = QueryParser::parse(trimmed);
    if (!pq.valid) {
        emit searchError(trimmed, pq.error);
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

        if (!matchEntry(e, pq, caseSensitive, pinyinEnabled))
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
