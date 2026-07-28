#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

struct QueryTerm {
    enum Kind {
        Name,      // match file name
        Path,      // match parent path / full path
        Regex,     // regex against name
        Ext        // extension list handled separately
    };

    Kind kind = Name;
    QString value;
    bool exclude = false;
};

struct ParsedQuery {
    QVector<QueryTerm> terms; // AND across terms; exclude terms filtered out
    QStringList extensions;   // empty = any; else OR match (case-insensitive)
    qint64 minSize = -1;      // bytes, -1 = unbound
    qint64 maxSize = -1;
    qint64 minMtime = -1;     // epoch seconds
    qint64 maxMtime = -1;
    bool valid = true;
    QString error;
};

class QueryParser
{
public:
    static ParsedQuery parse(const QString &input);
    static qint64 parseSizeToken(const QString &token, bool *ok = nullptr);
};
