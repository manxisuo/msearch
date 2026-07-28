#include "model/ResultModel.h"

#include <QDateTime>
#include <algorithm>

ResultModel::ResultModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ResultModel::setResults(const QVector<FileEntry> &results)
{
    beginResetModel();
    m_results = results;
    if (m_sortColumn >= 0 && !m_results.isEmpty()) {
        if (m_sortOrder == Qt::AscendingOrder) {
            std::stable_sort(m_results.begin(), m_results.end(),
                             [this](const FileEntry &a, const FileEntry &b) {
                                 return lessThan(a, b, m_sortColumn);
                             });
        } else {
            std::stable_sort(m_results.begin(), m_results.end(),
                             [this](const FileEntry &a, const FileEntry &b) {
                                 return lessThan(b, a, m_sortColumn);
                             });
        }
    }
    endResetModel();
}

void ResultModel::clear()
{
    beginResetModel();
    m_results.clear();
    endResetModel();
}

const FileEntry *ResultModel::entryAt(int row) const
{
    if (row < 0 || row >= m_results.size())
        return nullptr;
    return &m_results.at(row);
}

int ResultModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_results.size();
}

int ResultModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QString ResultModel::formatSize(qint64 bytes)
{
    if (bytes < 0)
        return QString();
    constexpr qint64 k = 1024;
    if (bytes < k)
        return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < k * k)
        return QString::number(bytes / double(k), 'f', 1) + QStringLiteral(" KB");
    if (bytes < k * k * k)
        return QString::number(bytes / double(k * k), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / double(k * k * k), 'f', 2) + QStringLiteral(" GB");
}

bool ResultModel::lessThan(const FileEntry &a, const FileEntry &b, int column)
{
    switch (column) {
    case Name:
        return QString::localeAwareCompare(a.name, b.name) < 0;
    case Path:
        return QString::localeAwareCompare(a.path, b.path) < 0;
    case Size: {
        // Directories sort before files when ascending by treating as -1
        const qint64 sa = a.isDir ? -1 : a.size;
        const qint64 sb = b.isDir ? -1 : b.size;
        return sa < sb;
    }
    case Modified:
        return a.mtime < b.mtime;
    default:
        return false;
    }
}

void ResultModel::sort(int column, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;
    if (m_results.isEmpty())
        return;

    beginResetModel();
    if (order == Qt::AscendingOrder) {
        std::stable_sort(m_results.begin(), m_results.end(),
                         [column](const FileEntry &a, const FileEntry &b) {
                             return lessThan(a, b, column);
                         });
    } else {
        std::stable_sort(m_results.begin(), m_results.end(),
                         [column](const FileEntry &a, const FileEntry &b) {
                             return lessThan(b, a, column);
                         });
    }
    endResetModel();
}

QVariant ResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return QVariant();

    const FileEntry &e = m_results.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Name:
            return e.name;
        case Path:
            return e.path;
        case Size:
            return e.isDir ? QStringLiteral("<DIR>") : formatSize(e.size);
        case Modified:
            return QDateTime::fromSecsSinceEpoch(e.mtime).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        default:
            break;
        }
    } else if (role == Qt::ToolTipRole) {
        return e.fullPath();
    }

    return QVariant();
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case Name:
        return tr("名称");
    case Path:
        return tr("路径");
    case Size:
        return tr("大小");
    case Modified:
        return tr("修改时间");
    default:
        return QVariant();
    }
}
