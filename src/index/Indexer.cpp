#include "index/Indexer.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegExp>
#include <QSet>
#include <QStack>

Indexer::Indexer(IndexDatabase *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
    m_excludeNames = QStringList()
        << QStringLiteral(".git")
        << QStringLiteral(".svn")
        << QStringLiteral(".hg")
        << QStringLiteral("node_modules")
        << QStringLiteral("__pycache__")
        << QStringLiteral(".cache")
        << QStringLiteral(".ccache");
}

void Indexer::setIncludePaths(const QStringList &paths)
{
    m_includePaths = paths;
}

void Indexer::setExcludeNames(const QStringList &names)
{
    m_excludeNames = names;
}

void Indexer::setExcludePatterns(const QStringList &patterns)
{
    m_excludePatterns = patterns;
}

void Indexer::setSkipHidden(bool on)
{
    m_skipHidden = on;
}

void Indexer::setFollowSymlinks(bool on)
{
    m_followSymlinks = on;
}

void Indexer::start()
{
    if (m_running.exchange(true))
        return;

    m_cancel = false;
    m_found = 0;

    if (m_includePaths.isEmpty()) {
        m_running = false;
        emit error(tr("未配置索引目录"));
        emit finished(false, 0);
        return;
    }

    m_db->clear();
    m_db->setIncludePaths(m_includePaths);

    for (const QString &root : m_includePaths) {
        if (m_cancel)
            break;
        walkDirectory(root);
    }

    const bool cancelled = m_cancel.load();
    m_running = false;
    emit finished(cancelled, m_found);
}

void Indexer::cancel()
{
    m_cancel = true;
}

bool Indexer::shouldSkipName(const QString &name) const
{
    if (m_excludeNames.contains(name))
        return true;
    if (m_skipHidden && name.startsWith(QLatin1Char('.')))
        return true;
    return false;
}

bool Indexer::shouldSkipPath(const QString &absolutePath) const
{
    for (const QString &pattern : m_excludePatterns) {
        const QString trimmed = pattern.trimmed();
        if (trimmed.isEmpty())
            continue;
        QRegExp rx(trimmed, Qt::CaseSensitive, QRegExp::Wildcard);
        if (rx.exactMatch(absolutePath) || rx.exactMatch(QFileInfo(absolutePath).fileName()))
            return true;
    }
    return false;
}

void Indexer::walkDirectory(const QString &rootPath)
{
    QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir())
        return;

    QVector<FileEntry> batch;
    batch.reserve(2048);
    QElapsedTimer timer;
    timer.start();

    auto flush = [this, &batch, &timer](const QString &hintPath) {
        if (batch.isEmpty())
            return;
        m_db->addBatch(batch);
        batch.clear();

        const qint64 elapsedMs = qMax(qint64(1), timer.elapsed());
        const double fps = m_found * 1000.0 / double(elapsedMs);
        emit progress(m_found, hintPath, fps);
    };

    auto appendEntry = [this, &batch, &flush](const QFileInfo &info) {
        FileEntry entry;
        entry.name = info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName();
        entry.path = info.absolutePath();
        entry.size = info.isDir() ? 0 : info.size();
        entry.mtime = info.lastModified().toSecsSinceEpoch();
        entry.isDir = info.isDir();
        batch.append(entry);
        ++m_found;
        if (batch.size() >= 2048)
            flush(entry.path);
    };

    appendEntry(rootInfo);

    QStack<QString> stack;
    stack.push(rootInfo.absoluteFilePath());
    QSet<QString> visited;
    visited.insert(rootInfo.canonicalFilePath());

    QDir::Filters filters = QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::System;
    if (!m_skipHidden)
        filters |= QDir::Hidden;

    while (!stack.isEmpty() && !m_cancel) {
        const QString dirPath = stack.pop();
        QDir dir(dirPath);
        const QFileInfoList entries = dir.entryInfoList(filters, QDir::Name);

        for (const QFileInfo &info : entries) {
            if (m_cancel)
                break;

            const QString name = info.fileName();
            if (shouldSkipName(name))
                continue;

            const QString abs = info.absoluteFilePath();
            if (abs.startsWith(QLatin1String("/proc"))
                || abs.startsWith(QLatin1String("/sys"))
                || abs.startsWith(QLatin1String("/dev"))
                || abs.startsWith(QLatin1String("/run"))) {
                continue;
            }

            if (shouldSkipPath(abs))
                continue;

            const bool isLink = info.isSymLink();
            if (isLink && !m_followSymlinks) {
                appendEntry(info);
                continue;
            }

            if (info.isDir()) {
                const QString canonical = info.canonicalFilePath();
                if (canonical.isEmpty() || visited.contains(canonical))
                    continue;
                visited.insert(canonical);
                appendEntry(info);
                stack.push(abs);
            } else {
                appendEntry(info);
            }
        }
    }

    flush(rootPath);
}
