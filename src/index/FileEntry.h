#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct FileEntry {
    QString name;
    QString path;      // directory path (parent), no trailing slash except root
    qint64 size = 0;
    qint64 mtime = 0;  // seconds since epoch
    bool isDir = false;

    QString fullPath() const
    {
        if (path == QLatin1String("/"))
            return path + name;
        return path + QLatin1Char('/') + name;
    }
};

Q_DECLARE_METATYPE(FileEntry)
Q_DECLARE_METATYPE(QVector<FileEntry>)
