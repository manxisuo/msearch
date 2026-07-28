#pragma once

#include "index/FileEntry.h"

#include <QAbstractTableModel>
#include <QVector>

class ResultModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        Name = 0,
        Path,
        Size,
        Modified,
        ColumnCount
    };

    explicit ResultModel(QObject *parent = nullptr);

    void setResults(const QVector<FileEntry> &results);
    void clear();

    const FileEntry *entryAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

private:
    static QString formatSize(qint64 bytes);
    static bool lessThan(const FileEntry &a, const FileEntry &b, int column);

    QVector<FileEntry> m_results;
    int m_sortColumn = Name;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};
