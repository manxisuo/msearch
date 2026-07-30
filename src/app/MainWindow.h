#pragma once

#include "app/IndexOptions.h"

#include <QMainWindow>
#include <QStringList>
#include <QSystemTrayIcon>
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
class QMenu;

class IndexDatabase;
class Indexer;
class SearchEngine;
class ResultModel;
class FsWatcher;
class GlobalHotkey;
struct FileEntry;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onQueryChanged(const QString &text);
    void onSearchTimeout();
    void onResultsReady(const QVector<FileEntry> &results, const QString &query, bool truncated);
    void onSearchError(const QString &query, const QString &error);
    void onRebuildIndex();
    void onCancelIndex();
    void onIndexProgress(qint64 filesFound, const QString &currentPath, double filesPerSec);
    void onIndexFinished(bool cancelled, qint64 totalFiles);
    void onLoadFinished(bool ok, int count, const QString &error);
    void onOpenSettings();
    void onFilterChanged(int index);
    void onCaseToggled(bool checked);
    void onPresetChosen(int index);
    void onBookmarkChosen(int index);
    void onSaveBookmark();
    void onShowHelp();
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void onWatchUpdated();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void showFromTray();
    void quitApp();
    void openSelected();
    void openSelectedFolder();
    void copySelectedPath();
    void focusSearch();
    void clearSearch();
    void persistIndexIfDirty();

private:
    void setupUi();
    void setupShortcuts();
    void setupTray();
    void setupHotkey();
    void loadSettings();
    void saveSettings();
    void loadOrBuildIndex();
    void startIndexing(bool clearAll = true);
    void startPartialIndex(const QStringList &pathsToAdd);
    void applyIndexerOptions();
    void applyWatcherOptions();
    void runSearch(const QString &query);
    void rememberQuery(const QString &query);
    void applyAutostart();
    void refreshBookmarkCombo();
    void applyStatusBarStyle();
    QIcon appIcon() const;
    QString indexFilePath() const;
    FileEntry currentEntry() const;

    QLineEdit *m_searchEdit = nullptr;
    QTableView *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QComboBox *m_bookmarkCombo = nullptr;
    QCheckBox *m_caseCheck = nullptr;
    QPushButton *m_rebuildBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_helpBtn = nullptr;
    QPushButton *m_bookmarkBtn = nullptr;
    QTimer *m_debounce = nullptr;
    QTimer *m_saveTimer = nullptr;
    QCompleter *m_completer = nullptr;
    QStringListModel *m_historyModel = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;

    ResultModel *m_model = nullptr;
    IndexDatabase *m_db = nullptr;
    Indexer *m_indexer = nullptr;
    SearchEngine *m_search = nullptr;
    FsWatcher *m_watcher = nullptr;
    GlobalHotkey *m_hotkey = nullptr;
    QThread *m_indexThread = nullptr;
    QThread *m_searchThread = nullptr;

    IndexOptions m_options;
    QStringList m_searchHistory;
    bool m_forceQuit = false;
};
