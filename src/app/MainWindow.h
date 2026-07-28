#pragma once

#include "app/IndexOptions.h"

#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QTableView;
class QLabel;
class QComboBox;
class QCheckBox;
class QPushButton;
class QTimer;
class QThread;
class QCompleter;
class QStringListModel;

class IndexDatabase;
class Indexer;
class SearchEngine;
class ResultModel;
struct FileEntry;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onQueryChanged(const QString &text);
    void onSearchTimeout();
    void onResultsReady(const QVector<FileEntry> &results, const QString &query, bool truncated);
    void onRebuildIndex();
    void onCancelIndex();
    void onIndexProgress(qint64 filesFound, const QString &currentPath, double filesPerSec);
    void onIndexFinished(bool cancelled, qint64 totalFiles);
    void onOpenSettings();
    void onFilterChanged(int index);
    void onCaseToggled(bool checked);
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void openSelected();
    void openSelectedFolder();
    void copySelectedPath();
    void focusSearch();
    void clearSearch();

private:
    void setupUi();
    void setupShortcuts();
    void loadSettings();
    void saveSettings();
    void loadOrBuildIndex();
    void startIndexing();
    void applyIndexerOptions();
    void runSearch(const QString &query);
    void rememberQuery(const QString &query);
    QString indexFilePath() const;
    FileEntry currentEntry() const;

    QLineEdit *m_searchEdit = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QCheckBox *m_caseCheck = nullptr;
    QPushButton *m_rebuildBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QTimer *m_debounce = nullptr;
    QCompleter *m_completer = nullptr;
    QStringListModel *m_historyModel = nullptr;

    ResultModel *m_model = nullptr;
    IndexDatabase *m_db = nullptr;
    Indexer *m_indexer = nullptr;
    SearchEngine *m_search = nullptr;
    QThread *m_indexThread = nullptr;
    QThread *m_searchThread = nullptr;

    IndexOptions m_options;
    QStringList m_searchHistory;
};
