#include "app/MainWindow.h"
#include "app/SettingsDialog.h"
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
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
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
    , m_indexThread(new QThread(this))
    , m_searchThread(new QThread(this))
{
    setupUi();
    setupShortcuts();
    loadSettings();

    m_indexer->moveToThread(m_indexThread);
    m_search->moveToThread(m_searchThread);

    connect(m_indexThread, &QThread::finished, m_indexer, &QObject::deleteLater);
    connect(m_searchThread, &QThread::finished, m_search, &QObject::deleteLater);

    connect(m_indexer, &Indexer::progress, this, &MainWindow::onIndexProgress);
    connect(m_indexer, &Indexer::finished, this, &MainWindow::onIndexFinished);
    connect(m_indexer, &Indexer::error, this, [this](const QString &msg) {
        m_statusLabel->setText(msg);
    });

    connect(m_search, &SearchEngine::resultsReady, this, &MainWindow::onResultsReady);

    m_indexThread->start();
    m_searchThread->start();

    loadOrBuildIndex();
}

MainWindow::~MainWindow()
{
    if (m_indexer)
        QMetaObject::invokeMethod(m_indexer, "cancel", Qt::QueuedConnection);

    m_indexThread->quit();
    m_searchThread->quit();
    m_indexThread->wait(3000);
    m_searchThread->wait(3000);

    delete m_db;
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("MSearch"));
    resize(960, 640);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *top = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("文件名关键字；支持通配符 * ?（如 *.pdf、report*）"));
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

    m_caseCheck = new QCheckBox(tr("区分大小写"), this);
    m_rebuildBtn = new QPushButton(tr("重建索引"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_cancelBtn->setEnabled(false);
    m_settingsBtn = new QPushButton(tr("设置"), this);

    top->addWidget(m_searchEdit, 1);
    top->addWidget(m_filterCombo);
    top->addWidget(m_caseCheck);
    top->addWidget(m_rebuildBtn);
    top->addWidget(m_cancelBtn);
    top->addWidget(m_settingsBtn);
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

    m_statusLabel = new QLabel(tr("就绪 — Ctrl+L 聚焦搜索，Esc 清空，F5 重建索引"), this);
    statusBar()->addWidget(m_statusLabel, 1);

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
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFilterChanged);
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
    m_options.maxResults = settings.value(QStringLiteral("maxResults"), 5000).toInt();

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
    settings.setValue(QStringLiteral("maxResults"), m_options.maxResults);
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
}

void MainWindow::loadOrBuildIndex()
{
    const QString path = indexFilePath();
    if (QFileInfo::exists(path) && m_db->loadFromFile(path)) {
        const QStringList saved = m_db->includePaths();
        if (!saved.isEmpty())
            m_options.includePaths = saved;
        m_statusLabel->setText(tr("已加载索引：%1 条 — 输入关键字开始搜索").arg(m_db->count()));
        if (!m_searchEdit->text().isEmpty())
            runSearch(m_searchEdit->text());
        return;
    }

    startIndexing();
}

void MainWindow::startIndexing()
{
    if (m_options.includePaths.isEmpty()) {
        QMessageBox::warning(this, tr("MSearch"), tr("请先在设置中添加要索引的目录。"));
        return;
    }

    m_rebuildBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_statusLabel->setText(tr("正在建立索引…"));

    applyIndexerOptions();
    QMetaObject::invokeMethod(m_indexer, "start", Qt::QueuedConnection);
}

void MainWindow::onRebuildIndex()
{
    startIndexing();
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

    if (!cancelled) {
        m_db->saveToFile(indexFilePath());
        m_statusLabel->setText(tr("索引完成：%1 条").arg(totalFiles));
    } else {
        m_statusLabel->setText(tr("索引已取消（当前 %1 条）").arg(m_db->count()));
        m_db->saveToFile(indexFilePath());
    }

    if (!m_searchEdit->text().trimmed().isEmpty())
        runSearch(m_searchEdit->text());
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(m_options, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const IndexOptions next = dlg.options();
    const bool needRebuild =
        next.includePaths != m_options.includePaths
        || next.excludePatterns != m_options.excludePatterns
        || next.skipHidden != m_options.skipHidden
        || next.followSymlinks != m_options.followSymlinks;

    m_options = next;
    saveSettings();

    if (!m_searchEdit->text().trimmed().isEmpty())
        runSearch(m_searchEdit->text());

    if (!needRebuild)
        return;

    const auto reply = QMessageBox::question(
        this,
        tr("重建索引"),
        tr("索引相关设置已更改，是否立即重建索引？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (reply == QMessageBox::Yes)
        startIndexing();
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
                              Q_ARG(int, m_options.maxResults));
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
        m_statusLabel->setText(tr("索引：%1 条 — 输入关键字开始搜索（支持 * ?）")
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}
