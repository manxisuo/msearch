#include "app/MainWindow.h"
#include "app/Autostart.h"
#include "app/GlobalHotkey.h"
#include "app/HelpDialog.h"
#include "app/SettingsDialog.h"
#include "index/FsWatcher.h"
#include "index/IndexDatabase.h"
#include "index/Indexer.h"
#include "model/ResultModel.h"
#include "search/SearchEngine.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringListModel>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_db(new IndexDatabase)
    , m_indexer(new Indexer(m_db))
    , m_search(new SearchEngine(m_db))
    , m_watcher(new FsWatcher(m_db, this))
    , m_hotkey(new GlobalHotkey(this))
    , m_indexThread(new QThread(this))
    , m_searchThread(new QThread(this))
{
    setupUi();
    setupShortcuts();
    setupTray();
    loadSettings();
    setupHotkey();
    applyAutostart();

    m_indexer->moveToThread(m_indexThread);
    m_search->moveToThread(m_searchThread);

    connect(m_indexThread, &QThread::finished, m_indexer, &QObject::deleteLater);
    connect(m_searchThread, &QThread::finished, m_search, &QObject::deleteLater);

    connect(m_indexer, &Indexer::progress, this, &MainWindow::onIndexProgress);
    connect(m_indexer, &Indexer::finished, this, &MainWindow::onIndexFinished);
    connect(m_indexer, &Indexer::loadFinished, this, &MainWindow::onLoadFinished);
    connect(m_indexer, &Indexer::error, this, [this](const QString &msg) {
        m_statusLabel->setText(msg);
    });

    connect(m_search, &SearchEngine::resultsReady, this, &MainWindow::onResultsReady);
    connect(m_search, &SearchEngine::searchError, this, &MainWindow::onSearchError);
    connect(m_watcher, &FsWatcher::indexUpdated, this, &MainWindow::onWatchUpdated);
    connect(m_watcher, &FsWatcher::statusMessage, this, [this](const QString &msg) {
        m_statusLabel->setText(msg);
    });
    connect(m_hotkey, &GlobalHotkey::activated, this, &MainWindow::showFromTray);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(2000);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::persistIndexIfDirty);

    m_indexThread->start();
    m_searchThread->start();

    loadOrBuildIndex();

    if (m_options.startInTray && m_tray)
        hide();
}

MainWindow::~MainWindow()
{
    m_watcher->stop();
    if (m_indexer)
        QMetaObject::invokeMethod(m_indexer, "cancel", Qt::QueuedConnection);

    persistIndexIfDirty();

    m_indexThread->quit();
    m_searchThread->quit();
    m_indexThread->wait(3000);
    m_searchThread->wait(3000);

    delete m_db;
}

QIcon MainWindow::appIcon() const
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(32, 110, 180));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(4, 4, 56, 56, 12, 12);
    p.setPen(QPen(Qt::white, 3));
    p.drawEllipse(16, 16, 24, 24);
    p.drawLine(36, 36, 48, 48);
    p.end();
    return QIcon(pm);
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("MSearch"));
    setWindowIcon(appIcon());
    resize(960, 640);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *top = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(
        tr("关键字 / 通配符 / ext: / path: / regex: / size: / dm:  — F1 查看语法"));
    m_searchEdit->setClearButtonEnabled(true);

    m_historyModel = new QStringListModel(this);
    m_completer = new QCompleter(m_historyModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_searchEdit->setCompleter(m_completer);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(tr("全部"), int(EntryFilter::All));
    m_filterCombo->addItem(tr("仅文件"), int(EntryFilter::FilesOnly));
    m_filterCombo->addItem(tr("仅文件夹"), int(EntryFilter::DirsOnly));

    m_presetCombo = new QComboBox(this);
    m_presetCombo->setMinimumWidth(120);
    m_presetCombo->addItem(tr("过滤器…"), QString());
    m_presetCombo->addItem(tr("PDF"), QStringLiteral("ext:pdf"));
    m_presetCombo->addItem(tr("图片"), QStringLiteral("ext:png;jpg;jpeg;gif;webp;bmp"));
    m_presetCombo->addItem(tr("视频"), QStringLiteral("ext:mp4;mkv;avi;mov;webm"));
    m_presetCombo->addItem(tr("音频"), QStringLiteral("ext:mp3;flac;wav;aac;ogg"));
    m_presetCombo->addItem(tr("文档"), QStringLiteral("ext:doc;docx;xls;xlsx;ppt;pptx;txt;md"));
    m_presetCombo->addItem(tr(">10MB"), QStringLiteral("size:>10mb"));
    m_presetCombo->addItem(tr("今天改过"), QStringLiteral("dm:today"));
    m_presetCombo->addItem(tr("近一周"), QStringLiteral("dm:week"));

    m_bookmarkCombo = new QComboBox(this);
    m_bookmarkCombo->setMinimumWidth(140);
    m_bookmarkCombo->setEditable(false);

    m_caseCheck = new QCheckBox(tr("区分大小写"), this);
    m_rebuildBtn = new QPushButton(tr("重建索引"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_cancelBtn->setEnabled(false);
    m_settingsBtn = new QPushButton(tr("设置"), this);
    m_helpBtn = new QPushButton(tr("?"), this);
    m_helpBtn->setFixedWidth(28);
    m_helpBtn->setToolTip(tr("搜索语法帮助 (F1)"));
    m_bookmarkBtn = new QPushButton(tr("★"), this);
    m_bookmarkBtn->setFixedWidth(28);
    m_bookmarkBtn->setToolTip(tr("将当前查询存为书签"));

    top->addWidget(m_searchEdit, 1);
    top->addWidget(m_presetCombo);
    top->addWidget(m_bookmarkCombo);
    top->addWidget(m_bookmarkBtn);
    top->addWidget(m_filterCombo);
    top->addWidget(m_caseCheck);
    top->addWidget(m_rebuildBtn);
    top->addWidget(m_cancelBtn);
    top->addWidget(m_settingsBtn);
    top->addWidget(m_helpBtn);
    root->addLayout(top);

    m_model = new ResultModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(ResultModel::Name, Qt::AscendingOrder);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionsClickable(true);
    m_table->horizontalHeader()->setSortIndicatorShown(true);
    m_table->horizontalHeader()->setSectionResizeMode(ResultModel::Name, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ResultModel::Path, QHeaderView::Stretch);
    m_table->setColumnWidth(ResultModel::Name, 220);
    m_table->setColumnWidth(ResultModel::Size, 100);
    m_table->setColumnWidth(ResultModel::Modified, 140);
    root->addWidget(m_table, 1);

    m_statusLabel = new QLabel(tr("就绪"), this);
    m_statusLabel->setAutoFillBackground(false);
    m_statusLabel->setForegroundRole(QPalette::WindowText);
    m_statusLabel->setBackgroundRole(QPalette::Window);
    statusBar()->addWidget(m_statusLabel, 1);
    applyStatusBarStyle();
    // UKUI may finish applying the theme after the first polish/show.
    QTimer::singleShot(0, this, [this]() { applyStatusBarStyle(); });
    QTimer::singleShot(100, this, [this]() { applyStatusBarStyle(); });

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(120);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onQueryChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        rememberQuery(m_searchEdit->text().trimmed());
    });
    connect(m_debounce, &QTimer::timeout, this, &MainWindow::onSearchTimeout);
    connect(m_rebuildBtn, &QPushButton::clicked, this, &MainWindow::onRebuildIndex);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelIndex);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
    connect(m_helpBtn, &QPushButton::clicked, this, &MainWindow::onShowHelp);
    connect(m_bookmarkBtn, &QPushButton::clicked, this, &MainWindow::onSaveBookmark);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFilterChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::onPresetChosen);
    connect(m_bookmarkCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::onBookmarkChosen);
    connect(m_caseCheck, &QCheckBox::toggled, this, &MainWindow::onCaseToggled);
    connect(m_table, &QTableView::doubleClicked, this, &MainWindow::onDoubleClicked);
    connect(m_table, &QTableView::customContextMenuRequested, this, &MainWindow::onContextMenu);

    auto *openAct = new QAction(tr("打开"), this);
    openAct->setShortcut(QKeySequence(Qt::Key_Return));
    connect(openAct, &QAction::triggered, this, &MainWindow::openSelected);
    addAction(openAct);

    m_searchEdit->setFocus();
}

void MainWindow::setupShortcuts()
{
    auto *focusAct = new QAction(this);
    focusAct->setShortcuts({QKeySequence(QStringLiteral("Ctrl+L")),
                            QKeySequence(QStringLiteral("Ctrl+F"))});
    connect(focusAct, &QAction::triggered, this, &MainWindow::focusSearch);
    addAction(focusAct);

    auto *clearAct = new QAction(this);
    clearAct->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(clearAct, &QAction::triggered, this, &MainWindow::clearSearch);
    addAction(clearAct);

    auto *rebuildAct = new QAction(this);
    rebuildAct->setShortcut(QKeySequence(Qt::Key_F5));
    connect(rebuildAct, &QAction::triggered, this, &MainWindow::onRebuildIndex);
    addAction(rebuildAct);

    auto *helpAct = new QAction(this);
    helpAct->setShortcut(QKeySequence(Qt::Key_F1));
    connect(helpAct, &QAction::triggered, this, &MainWindow::onShowHelp);
    addAction(helpAct);
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction(tr("显示主窗口"), this, &MainWindow::showFromTray);
    m_trayMenu->addAction(tr("重建索引"), this, &MainWindow::onRebuildIndex);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("退出"), this, &MainWindow::quitApp);

    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setToolTip(QStringLiteral("MSearch"));
    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_tray->show();
}

void MainWindow::setupHotkey()
{
    if (m_options.hotkey.isEmpty()) {
        m_hotkey->clear();
        return;
    }
    if (!m_hotkey->setShortcut(m_options.hotkey)) {
        m_statusLabel->setText(tr("全局热键注册失败：%1（可能被占用）").arg(m_options.hotkey));
    }
}

void MainWindow::applyAutostart()
{
    Autostart::setEnabled(m_options.autostart, QCoreApplication::applicationFilePath());
}

void MainWindow::loadSettings()
{
    QSettings settings(QStringLiteral("MSearch"), QStringLiteral("MSearch"));
    m_options.includePaths = settings.value(QStringLiteral("includePaths")).toStringList();
    if (m_options.includePaths.isEmpty()) {
        const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        if (!home.isEmpty())
            m_options.includePaths << home;
    }

    m_options.excludePatterns = settings.value(QStringLiteral("excludePatterns")).toStringList();
    m_options.skipHidden = settings.value(QStringLiteral("skipHidden"), false).toBool();
    m_options.followSymlinks = settings.value(QStringLiteral("followSymlinks"), false).toBool();
    m_options.skipNetworkMounts = settings.value(QStringLiteral("skipNetworkMounts"), true).toBool();
    m_options.skipReadOnlyMounts = settings.value(QStringLiteral("skipReadOnlyMounts"), false).toBool();
    m_options.maxResults = settings.value(QStringLiteral("maxResults"), 5000).toInt();
    m_options.watchFilesystem = settings.value(QStringLiteral("watchFilesystem"), true).toBool();
    m_options.minimizeToTray = settings.value(QStringLiteral("minimizeToTray"), true).toBool();
    m_options.startInTray = settings.value(QStringLiteral("startInTray"), false).toBool();
    m_options.autostart = settings.value(QStringLiteral("autostart"), false).toBool();
    m_options.hotkey = settings.value(QStringLiteral("hotkey"),
                                      QStringLiteral("Ctrl+Alt+Space")).toString();
    m_options.pinyinEnabled = settings.value(QStringLiteral("pinyinEnabled"), true).toBool();
    m_options.bookmarks = settings.value(QStringLiteral("bookmarks")).toStringList();
    refreshBookmarkCombo();

    m_caseCheck->setChecked(settings.value(QStringLiteral("caseSensitive"), false).toBool());
    const int filter = settings.value(QStringLiteral("filter"), 0).toInt();
    if (filter >= 0 && filter < m_filterCombo->count())
        m_filterCombo->setCurrentIndex(filter);

    m_searchHistory = settings.value(QStringLiteral("searchHistory")).toStringList();
    m_historyModel->setStringList(m_searchHistory);

    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings settings(QStringLiteral("MSearch"), QStringLiteral("MSearch"));
    settings.setValue(QStringLiteral("includePaths"), m_options.includePaths);
    settings.setValue(QStringLiteral("excludePatterns"), m_options.excludePatterns);
    settings.setValue(QStringLiteral("skipHidden"), m_options.skipHidden);
    settings.setValue(QStringLiteral("followSymlinks"), m_options.followSymlinks);
    settings.setValue(QStringLiteral("skipNetworkMounts"), m_options.skipNetworkMounts);
    settings.setValue(QStringLiteral("skipReadOnlyMounts"), m_options.skipReadOnlyMounts);
    settings.setValue(QStringLiteral("maxResults"), m_options.maxResults);
    settings.setValue(QStringLiteral("watchFilesystem"), m_options.watchFilesystem);
    settings.setValue(QStringLiteral("minimizeToTray"), m_options.minimizeToTray);
    settings.setValue(QStringLiteral("startInTray"), m_options.startInTray);
    settings.setValue(QStringLiteral("autostart"), m_options.autostart);
    settings.setValue(QStringLiteral("hotkey"), m_options.hotkey);
    settings.setValue(QStringLiteral("pinyinEnabled"), m_options.pinyinEnabled);
    settings.setValue(QStringLiteral("bookmarks"), m_options.bookmarks);
    settings.setValue(QStringLiteral("caseSensitive"), m_caseCheck->isChecked());
    settings.setValue(QStringLiteral("filter"), m_filterCombo->currentIndex());
    settings.setValue(QStringLiteral("searchHistory"), m_searchHistory);
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
}

QString MainWindow::indexFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/index.msdb");
}

void MainWindow::applyIndexerOptions()
{
    m_indexer->setIncludePaths(m_options.includePaths);
    m_indexer->setExcludePatterns(m_options.excludePatterns);
    m_indexer->setSkipHidden(m_options.skipHidden);
    m_indexer->setFollowSymlinks(m_options.followSymlinks);
    m_indexer->setSkipNetworkMounts(m_options.skipNetworkMounts);
    m_indexer->setSkipReadOnlyMounts(m_options.skipReadOnlyMounts);
}

void MainWindow::applyWatcherOptions()
{
    m_watcher->stop();
    if (!m_options.watchFilesystem)
        return;

    m_watcher->setExcludePatterns(m_options.excludePatterns);
    m_watcher->setSkipHidden(m_options.skipHidden);
    m_watcher->setFollowSymlinks(m_options.followSymlinks);
    m_watcher->setSkipNetworkMounts(m_options.skipNetworkMounts);
    m_watcher->setSkipReadOnlyMounts(m_options.skipReadOnlyMounts);
    m_watcher->rebuildWatches();
}

void MainWindow::loadOrBuildIndex()
{
    const QString path = indexFilePath();
    if (!QFileInfo::exists(path)) {
        startIndexing(true);
        return;
    }

    m_statusLabel->setText(tr("正在异步加载索引…"));
    QMetaObject::invokeMethod(m_indexer, "loadFromFile", Qt::QueuedConnection,
                              Q_ARG(QString, path));
}

void MainWindow::onLoadFinished(bool ok, int count, const QString &error)
{
    if (!ok) {
        m_statusLabel->setText(tr("索引损坏，将重新建立：%1").arg(error));
        if (m_tray)
            m_tray->showMessage(tr("MSearch"), tr("索引文件损坏，正在重建…"),
                                QSystemTrayIcon::Warning, 3000);
        startIndexing(true);
        return;
    }

    const QStringList saved = m_db->includePaths();
    if (!saved.isEmpty())
        m_options.includePaths = saved;

    m_statusLabel->setText(tr("已加载索引：%1 条 — 输入关键字开始搜索").arg(count));
    applyWatcherOptions();

    if (!m_searchEdit->text().isEmpty())
        runSearch(m_searchEdit->text());
}

void MainWindow::startIndexing(bool clearAll)
{
    if (m_options.includePaths.isEmpty()) {
        QMessageBox::warning(this, tr("MSearch"), tr("请先在设置中添加要索引的目录。"));
        return;
    }

    m_watcher->stop();
    m_rebuildBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_statusLabel->setText(tr("正在建立索引…"));

    applyIndexerOptions();
    m_indexer->setClearBeforeIndex(clearAll);
    QMetaObject::invokeMethod(m_indexer, "start", Qt::QueuedConnection);
}

void MainWindow::startPartialIndex(const QStringList &pathsToAdd)
{
    if (pathsToAdd.isEmpty()) {
        applyWatcherOptions();
        return;
    }

    m_watcher->stop();
    m_rebuildBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_statusLabel->setText(tr("正在增量索引新目录…"));

    m_indexer->setIncludePaths(pathsToAdd);
    m_indexer->setExcludePatterns(m_options.excludePatterns);
    m_indexer->setSkipHidden(m_options.skipHidden);
    m_indexer->setFollowSymlinks(m_options.followSymlinks);
    m_indexer->setSkipNetworkMounts(m_options.skipNetworkMounts);
    m_indexer->setSkipReadOnlyMounts(m_options.skipReadOnlyMounts);
    m_indexer->setClearBeforeIndex(false);
    // Keep full include path list on DB after merge
    m_db->setIncludePaths(m_options.includePaths);
    QMetaObject::invokeMethod(m_indexer, "start", Qt::QueuedConnection);
}

void MainWindow::onRebuildIndex()
{
    startIndexing(true);
}

void MainWindow::onCancelIndex()
{
    QMetaObject::invokeMethod(m_indexer, "cancel", Qt::QueuedConnection);
}

void MainWindow::onIndexProgress(qint64 filesFound, const QString &currentPath, double filesPerSec)
{
    m_statusLabel->setText(tr("索引中：%1 条（%2/秒）— %3")
                               .arg(filesFound)
                               .arg(filesPerSec, 0, 'f', 0)
                               .arg(currentPath));
}

void MainWindow::onIndexFinished(bool cancelled, qint64 totalFiles)
{
    m_rebuildBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_db->setIncludePaths(m_options.includePaths);

    if (!cancelled) {
        m_db->saveToFile(indexFilePath());
        m_db->markClean();
        m_statusLabel->setText(tr("索引完成：共 %1 条（本次扫描 %2）")
                                   .arg(m_db->count())
                                   .arg(totalFiles));
    } else {
        m_statusLabel->setText(tr("索引已取消（当前 %1 条）").arg(m_db->count()));
        m_db->saveToFile(indexFilePath());
        m_db->markClean();
    }

    applyWatcherOptions();

    if (!m_searchEdit->text().trimmed().isEmpty())
        runSearch(m_searchEdit->text());
}

void MainWindow::onOpenSettings()
{
    const IndexOptions previous = m_options;
    SettingsDialog dlg(m_options, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_options = dlg.options();
    saveSettings();
    setupHotkey();
    applyAutostart();

    if (!m_searchEdit->text().trimmed().isEmpty())
        runSearch(m_searchEdit->text());

    const bool scanOptsChanged =
        previous.excludePatterns != m_options.excludePatterns
        || previous.skipHidden != m_options.skipHidden
        || previous.followSymlinks != m_options.followSymlinks
        || previous.skipNetworkMounts != m_options.skipNetworkMounts
        || previous.skipReadOnlyMounts != m_options.skipReadOnlyMounts;

    QStringList added;
    for (const QString &p : m_options.includePaths) {
        if (!previous.includePaths.contains(p))
            added << p;
    }
    QStringList removed;
    for (const QString &p : previous.includePaths) {
        if (!m_options.includePaths.contains(p))
            removed << p;
    }

    if (scanOptsChanged) {
        const auto reply = QMessageBox::question(
            this, tr("重建索引"),
            tr("扫描相关设置已更改，是否立即全量重建索引？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::Yes)
            startIndexing(true);
        else
            applyWatcherOptions();
        return;
    }

    for (const QString &p : removed)
        m_db->removeUnderPrefix(p);

    if (!removed.isEmpty()) {
        m_db->setIncludePaths(m_options.includePaths);
        persistIndexIfDirty();
    }

    if (!added.isEmpty()) {
        const auto reply = QMessageBox::question(
            this, tr("增量索引"),
            tr("检测到新的索引目录，是否立即扫描这些目录？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::Yes) {
            startPartialIndex(added);
            return;
        }
    }

    applyWatcherOptions();
    if (!m_searchEdit->text().trimmed().isEmpty())
        runSearch(m_searchEdit->text());
}

void MainWindow::onQueryChanged(const QString &)
{
    m_debounce->start();
}

void MainWindow::onSearchTimeout()
{
    runSearch(m_searchEdit->text());
}

void MainWindow::runSearch(const QString &query)
{
    QMetaObject::invokeMethod(m_search, "search", Qt::QueuedConnection,
                              Q_ARG(QString, query),
                              Q_ARG(bool, m_caseCheck->isChecked()),
                              Q_ARG(int, m_filterCombo->currentData().toInt()),
                              Q_ARG(int, m_options.maxResults),
                              Q_ARG(bool, m_options.pinyinEnabled));
}

void MainWindow::rememberQuery(const QString &query)
{
    if (query.isEmpty())
        return;
    m_searchHistory.removeAll(query);
    m_searchHistory.prepend(query);
    while (m_searchHistory.size() > 20)
        m_searchHistory.removeLast();
    m_historyModel->setStringList(m_searchHistory);
}

void MainWindow::onResultsReady(const QVector<FileEntry> &results, const QString &query, bool truncated)
{
    if (query != m_searchEdit->text().trimmed())
        return;

    m_model->setResults(results);
    if (query.isEmpty()) {
        m_statusLabel->setText(tr("索引：%1 条 — 输入关键字开始搜索（F1 查看语法）")
                                   .arg(m_db->count()));
    } else if (truncated) {
        m_statusLabel->setText(tr("显示前 %1 条（已达上限，可在设置中调整）— 索引共 %2 条")
                                   .arg(results.size())
                                   .arg(m_db->count()));
    } else {
        m_statusLabel->setText(tr("找到 %1 条 — 索引共 %2 条")
                                   .arg(results.size())
                                   .arg(m_db->count()));
    }
}

void MainWindow::onSearchError(const QString &query, const QString &error)
{
    if (query != m_searchEdit->text().trimmed())
        return;
    m_model->clear();
    m_statusLabel->setText(tr("查询无效：%1").arg(error));
}

void MainWindow::onPresetChosen(int index)
{
    if (index <= 0)
        return;
    const QString frag = m_presetCombo->itemData(index).toString();
    if (frag.isEmpty())
        return;

    QString cur = m_searchEdit->text().trimmed();
    if (!cur.isEmpty())
        cur += QLatin1Char(' ');
    cur += frag;
    m_searchEdit->setText(cur);
    m_presetCombo->setCurrentIndex(0);
    m_searchEdit->setFocus();
}

void MainWindow::refreshBookmarkCombo()
{
    m_bookmarkCombo->blockSignals(true);
    m_bookmarkCombo->clear();
    m_bookmarkCombo->addItem(tr("书签…"));
    for (const QString &b : m_options.bookmarks)
        m_bookmarkCombo->addItem(b);
    m_bookmarkCombo->blockSignals(false);
}

void MainWindow::onBookmarkChosen(int index)
{
    if (index <= 0)
        return;
    m_searchEdit->setText(m_bookmarkCombo->itemText(index));
    m_bookmarkCombo->setCurrentIndex(0);
    m_searchEdit->setFocus();
}

void MainWindow::onSaveBookmark()
{
    const QString q = m_searchEdit->text().trimmed();
    if (q.isEmpty())
        return;
    if (!m_options.bookmarks.contains(q)) {
        m_options.bookmarks.prepend(q);
        while (m_options.bookmarks.size() > 30)
            m_options.bookmarks.removeLast();
        refreshBookmarkCombo();
        saveSettings();
    }
    m_statusLabel->setText(tr("已保存书签：%1").arg(q));
}

void MainWindow::onShowHelp()
{
    HelpDialog dlg(this);
    dlg.exec();
}

void MainWindow::onWatchUpdated()
{
    m_saveTimer->start();
    if (!m_searchEdit->text().trimmed().isEmpty())
        runSearch(m_searchEdit->text());
    else
        m_statusLabel->setText(tr("索引已增量更新：%1 条").arg(m_db->count()));
}

void MainWindow::persistIndexIfDirty()
{
    if (!m_db->isDirty())
        return;
    if (m_db->saveToFile(indexFilePath()))
        m_db->markClean();
}

void MainWindow::onFilterChanged(int)
{
    runSearch(m_searchEdit->text());
}

void MainWindow::onCaseToggled(bool)
{
    runSearch(m_searchEdit->text());
}

void MainWindow::focusSearch()
{
    showFromTray();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void MainWindow::clearSearch()
{
    if (m_searchEdit->hasFocus() || !m_searchEdit->text().isEmpty()) {
        m_searchEdit->clear();
        m_searchEdit->setFocus();
    }
}

void MainWindow::showFromTray()
{
    showNormal();
    raise();
    activateWindow();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void MainWindow::quitApp()
{
    m_forceQuit = true;
    close();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible())
            hide();
        else
            showFromTray();
    }
}

FileEntry MainWindow::currentEntry() const
{
    const QModelIndex idx = m_table->currentIndex();
    const FileEntry *e = m_model->entryAt(idx.row());
    return e ? *e : FileEntry();
}

void MainWindow::onDoubleClicked(const QModelIndex &)
{
    openSelected();
}

void MainWindow::onContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid())
        return;
    m_table->setCurrentIndex(idx);

    QMenu menu(this);
    menu.addAction(tr("打开"), this, &MainWindow::openSelected);
    menu.addAction(tr("打开所在文件夹"), this, &MainWindow::openSelectedFolder);
    menu.addAction(tr("复制完整路径"), this, &MainWindow::copySelectedPath);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void MainWindow::openSelected()
{
    const FileEntry e = currentEntry();
    if (e.name.isEmpty())
        return;
    rememberQuery(m_searchEdit->text().trimmed());
    QDesktopServices::openUrl(QUrl::fromLocalFile(e.fullPath()));
}

void MainWindow::openSelectedFolder()
{
    const FileEntry e = currentEntry();
    if (e.name.isEmpty())
        return;
    const QString target = e.isDir ? e.fullPath() : e.path;
    QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}

void MainWindow::copySelectedPath()
{
    const FileEntry e = currentEntry();
    if (e.name.isEmpty())
        return;
    QApplication::clipboard()->setText(e.fullPath());
    m_statusLabel->setText(tr("已复制：%1").arg(e.fullPath()));
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && m_options.minimizeToTray && m_tray) {
            QTimer::singleShot(0, this, [this]() { hide(); });
        }
    } else if (event->type() == QEvent::PaletteChange
               || event->type() == QEvent::StyleChange) {
        applyStatusBarStyle();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::applyStatusBarStyle()
{
    if (!m_statusLabel || !statusBar())
        return;

    // UKUI/Kylin: status bar often looks dark while QLabel keeps black text.
    auto luma = [](const QColor &c) {
        return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF();
    };

    const QPalette appPal = palette();
    const QPalette sbPalIn = statusBar()->palette();
    QColor bg = sbPalIn.color(QPalette::Window);
    const QColor candidates[] = {
        sbPalIn.color(QPalette::Window),
        sbPalIn.color(QPalette::Button),
        sbPalIn.color(QPalette::Mid),
        appPal.color(QPalette::Window),
        appPal.color(QPalette::Base),
    };
    for (const QColor &c : candidates) {
        if (c.isValid() && luma(c) < luma(bg))
            bg = c;
    }

    const QColor themeText = appPal.color(QPalette::WindowText);
    const bool themeLooksDark =
        luma(themeText) > 0.6 || luma(appPal.color(QPalette::Window)) < 0.55;
    if (themeLooksDark && luma(bg) > 0.5)
        bg = QColor(45, 45, 45);

    const bool dark = themeLooksDark || luma(bg) < 0.55;
    const QColor fg = dark ? QColor(235, 235, 235) : QColor(30, 30, 30);
    if (dark && luma(bg) > 0.45)
        bg = QColor(40, 40, 40);

    QPalette sbPal = statusBar()->palette();
    sbPal.setColor(QPalette::Window, bg);
    sbPal.setColor(QPalette::Base, bg);
    sbPal.setColor(QPalette::Button, bg);
    sbPal.setColor(QPalette::WindowText, fg);
    sbPal.setColor(QPalette::Text, fg);
    sbPal.setColor(QPalette::ButtonText, fg);
    statusBar()->setPalette(sbPal);
    statusBar()->setAutoFillBackground(true);

    QPalette labelPal = m_statusLabel->palette();
    labelPal.setColor(QPalette::WindowText, fg);
    labelPal.setColor(QPalette::Text, fg);
    labelPal.setColor(QPalette::Window, bg);
    m_statusLabel->setPalette(labelPal);
    m_statusLabel->setForegroundRole(QPalette::WindowText);
    m_statusLabel->setAutoFillBackground(false);

    // Explicit hex colors: more reliable than palette() under UKUI.
    statusBar()->setStyleSheet(
        QStringLiteral(
            "QStatusBar {"
            "  color: %1;"
            "  background-color: %2;"
            "}"
            "QStatusBar::item { border: none; }"
            "QStatusBar QLabel {"
            "  color: %1;"
            "  background-color: transparent;"
            "}")
            .arg(fg.name(), bg.name()));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_forceQuit && m_options.minimizeToTray && m_tray) {
        hide();
        event->ignore();
        return;
    }

    saveSettings();
    persistIndexIfDirty();
    QMainWindow::closeEvent(event);
}
