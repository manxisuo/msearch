#include "app/SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const IndexOptions &options, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("设置"));
    resize(560, 520);

    auto *root = new QVBoxLayout(this);

    auto *pathGroup = new QGroupBox(tr("索引目录"), this);
    auto *pathLayout = new QVBoxLayout(pathGroup);
    pathLayout->addWidget(new QLabel(tr("将扫描以下目录（含子目录）："), this));
    m_list = new QListWidget(this);
    m_list->addItems(options.includePaths);
    pathLayout->addWidget(m_list, 1);

    auto *row = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("添加目录…"), this);
    auto *removeBtn = new QPushButton(tr("移除"), this);
    row->addWidget(addBtn);
    row->addWidget(removeBtn);
    row->addStretch(1);
    pathLayout->addLayout(row);
    root->addWidget(pathGroup, 1);

    auto *excludeGroup = new QGroupBox(tr("排除规则"), this);
    auto *excludeLayout = new QVBoxLayout(excludeGroup);
    excludeLayout->addWidget(new QLabel(
        tr("每行一条通配符，匹配文件名或完整路径（如 *.o、*/build/*）："), this));
    m_excludeEdit = new QPlainTextEdit(this);
    m_excludeEdit->setPlainText(options.excludePatterns.join(QLatin1Char('\n')));
    m_excludeEdit->setMaximumHeight(100);
    excludeLayout->addWidget(m_excludeEdit);
    root->addWidget(excludeGroup);

    auto *optGroup = new QGroupBox(tr("扫描与搜索"), this);
    auto *form = new QFormLayout(optGroup);
    m_skipHidden = new QCheckBox(tr("跳过隐藏文件/目录（以 . 开头）"), this);
    m_skipHidden->setChecked(options.skipHidden);
    m_followSymlinks = new QCheckBox(tr("跟随符号链接"), this);
    m_followSymlinks->setChecked(options.followSymlinks);
    m_maxResults = new QSpinBox(this);
    m_maxResults->setRange(100, 1000000);
    m_maxResults->setSingleStep(500);
    m_maxResults->setValue(options.maxResults);
    form->addRow(m_skipHidden);
    form->addRow(m_followSymlinks);
    form->addRow(tr("搜索结果上限"), m_maxResults);
    root->addWidget(optGroup);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addPath);
    connect(removeBtn, &QPushButton::clicked, this, &SettingsDialog::removeSelected);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

IndexOptions SettingsDialog::options() const
{
    IndexOptions opt;
    for (int i = 0; i < m_list->count(); ++i)
        opt.includePaths << m_list->item(i)->text();

    const QStringList lines = m_excludeEdit->toPlainText().split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.isEmpty())
            opt.excludePatterns << line;
    }

    opt.skipHidden = m_skipHidden->isChecked();
    opt.followSymlinks = m_followSymlinks->isChecked();
    opt.maxResults = m_maxResults->value();
    return opt;
}

void SettingsDialog::addPath()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("选择索引目录"));
    if (dir.isEmpty())
        return;

    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->text() == dir)
            return;
    }
    m_list->addItem(dir);
}

void SettingsDialog::removeSelected()
{
    qDeleteAll(m_list->selectedItems());
}
