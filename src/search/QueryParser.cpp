#include "search/QueryParser.h"

#include <QDate>
#include <QDateTime>
#include <QRegExp>
#include <QTime>

static QStringList splitTerms(const QString &input)
{
    QStringList out;
    QString cur;
    bool inQuote = false;
    for (int i = 0; i < input.size(); ++i) {
        const QChar c = input.at(i);
        if (c == QLatin1Char('"')) {
            inQuote = !inQuote;
            continue;
        }
        if (!inQuote && c.isSpace()) {
            if (!cur.isEmpty()) {
                out << cur;
                cur.clear();
            }
            continue;
        }
        cur.append(c);
    }
    if (!cur.isEmpty())
        out << cur;
    return out;
}

qint64 QueryParser::parseSizeToken(const QString &token, bool *ok)
{
    if (ok)
        *ok = false;
    QString t = token.trimmed().toLower();
    if (t.isEmpty())
        return -1;

    qint64 mul = 1;
    if (t.endsWith(QLatin1String("kb")) || t.endsWith(QLatin1Char('k'))) {
        mul = 1024;
        t.chop(t.endsWith(QLatin1String("kb")) ? 2 : 1);
    } else if (t.endsWith(QLatin1String("mb")) || t.endsWith(QLatin1Char('m'))) {
        mul = 1024LL * 1024;
        t.chop(t.endsWith(QLatin1String("mb")) ? 2 : 1);
    } else if (t.endsWith(QLatin1String("gb")) || t.endsWith(QLatin1Char('g'))) {
        mul = 1024LL * 1024 * 1024;
        t.chop(t.endsWith(QLatin1String("gb")) ? 2 : 1);
    } else if (t.endsWith(QLatin1Char('b'))) {
        t.chop(1);
    }

    bool numOk = false;
    const double v = t.toDouble(&numOk);
    if (!numOk)
        return -1;
    if (ok)
        *ok = true;
    return qint64(v * mul);
}

static bool parseSizeFilter(const QString &body, ParsedQuery *q)
{
    const QString b = body.trimmed();
    if (b.contains(QLatin1String(".."))) {
        const QStringList parts = b.split(QStringLiteral(".."));
        if (parts.size() != 2)
            return false;
        bool ok1 = false, ok2 = false;
        q->minSize = QueryParser::parseSizeToken(parts[0], &ok1);
        q->maxSize = QueryParser::parseSizeToken(parts[1], &ok2);
        return ok1 && ok2;
    }
    if (b.startsWith(QLatin1String(">="))) {
        bool ok = false;
        q->minSize = QueryParser::parseSizeToken(b.mid(2), &ok);
        return ok;
    }
    if (b.startsWith(QLatin1String("<="))) {
        bool ok = false;
        q->maxSize = QueryParser::parseSizeToken(b.mid(2), &ok);
        return ok;
    }
    if (b.startsWith(QLatin1Char('>'))) {
        bool ok = false;
        q->minSize = QueryParser::parseSizeToken(b.mid(1), &ok);
        if (ok)
            q->minSize += 1;
        return ok;
    }
    if (b.startsWith(QLatin1Char('<'))) {
        bool ok = false;
        q->maxSize = QueryParser::parseSizeToken(b.mid(1), &ok);
        if (ok && q->maxSize > 0)
            q->maxSize -= 1;
        return ok;
    }
    bool ok = false;
    const qint64 exact = QueryParser::parseSizeToken(b, &ok);
    if (!ok)
        return false;
    q->minSize = exact;
    q->maxSize = exact;
    return true;
}

static bool parseDateFilter(const QString &body, ParsedQuery *q)
{
    const QString b = body.trimmed().toLower();

    auto dayStart = [](const QDate &d) -> qint64 {
        return QDateTime(d, QTime(0, 0)).toSecsSinceEpoch();
    };

    if (b == QLatin1String("today")) {
        q->minMtime = dayStart(QDate::currentDate());
        return true;
    }
    if (b == QLatin1String("yesterday")) {
        q->minMtime = dayStart(QDate::currentDate().addDays(-1));
        q->maxMtime = dayStart(QDate::currentDate()) - 1;
        return true;
    }
    if (b == QLatin1String("week") || b == QLatin1String("thisweek")) {
        q->minMtime = dayStart(QDate::currentDate().addDays(-7));
        return true;
    }
    if (b == QLatin1String("month") || b == QLatin1String("thismonth")) {
        q->minMtime = dayStart(QDate::currentDate().addMonths(-1));
        return true;
    }

    QString rest = b;
    bool maxSide = false;
    bool minSide = false;
    if (rest.startsWith(QLatin1String(">="))) {
        minSide = true;
        rest = rest.mid(2);
    } else if (rest.startsWith(QLatin1String("<="))) {
        maxSide = true;
        rest = rest.mid(2);
    } else if (rest.startsWith(QLatin1Char('>'))) {
        minSide = true;
        rest = rest.mid(1);
    } else if (rest.startsWith(QLatin1Char('<'))) {
        maxSide = true;
        rest = rest.mid(1);
    }

    const QDate date = QDate::fromString(rest.trimmed(), QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid())
        return false;
    const qint64 ts = dayStart(date);
    if (minSide) {
        q->minMtime = ts;
        return true;
    }
    if (maxSide) {
        q->maxMtime = ts + 24 * 3600 - 1;
        return true;
    }
    q->minMtime = ts;
    q->maxMtime = ts + 24 * 3600 - 1;
    return true;
}

ParsedQuery QueryParser::parse(const QString &input)
{
    ParsedQuery q;
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return q;

    const QStringList parts = splitTerms(trimmed);
    for (QString part : parts) {
        bool exclude = false;
        if (part.startsWith(QLatin1Char('-')) && part.size() > 1) {
            exclude = true;
            part = part.mid(1);
        }

        const int colon = part.indexOf(QLatin1Char(':'));
        const QString key = colon > 0 ? part.left(colon).toLower() : QString();
        const QString body = colon > 0 ? part.mid(colon + 1) : part;

        if (key == QLatin1String("ext") || key == QLatin1String("e")) {
            const QStringList exts = body.split(QRegExp(QStringLiteral("[;,\\s]+")),
                                                QString::SkipEmptyParts);
            for (QString e : exts) {
                if (e.startsWith(QLatin1Char('.')))
                    e = e.mid(1);
                if (e.isEmpty())
                    continue;
                if (exclude) {
                    QueryTerm t;
                    t.kind = QueryTerm::Name;
                    t.value = QStringLiteral("*.") + e;
                    t.exclude = true;
                    q.terms.append(t);
                } else {
                    q.extensions.append(e.toLower());
                }
            }
            continue;
        }

        if (key == QLatin1String("size") || key == QLatin1String("s")) {
            if (exclude) {
                q.valid = false;
                q.error = QStringLiteral("size: does not support exclude (-)");
                return q;
            }
            if (!parseSizeFilter(body, &q)) {
                q.valid = false;
                q.error = QStringLiteral("invalid size:") + body;
                return q;
            }
            continue;
        }

        if (key == QLatin1String("dm") || key == QLatin1String("date")
            || key == QLatin1String("modified")) {
            if (exclude) {
                q.valid = false;
                q.error = QStringLiteral("dm: does not support exclude (-)");
                return q;
            }
            if (!parseDateFilter(body, &q)) {
                q.valid = false;
                q.error = QStringLiteral("invalid dm:") + body;
                return q;
            }
            continue;
        }

        QueryTerm term;
        term.exclude = exclude;
        if (key == QLatin1String("regex") || key == QLatin1String("r") || key == QLatin1String("re")) {
            term.kind = QueryTerm::Regex;
            term.value = body;
        } else if (key == QLatin1String("path") || key == QLatin1String("p")) {
            term.kind = QueryTerm::Path;
            term.value = body;
        } else if (key == QLatin1String("name") || key == QLatin1String("n")) {
            term.kind = QueryTerm::Name;
            term.value = body;
        } else if (key.isEmpty()) {
            term.kind = QueryTerm::Name;
            term.value = body;
        } else {
            // Unknown prefix: match whole token as name (including colon)
            term.kind = QueryTerm::Name;
            term.value = part;
        }

        if (term.value.isEmpty())
            continue;
        q.terms.append(term);
    }

    return q;
}
