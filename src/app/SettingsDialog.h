#pragma once

#include "app/IndexOptions.h"

#include <QDialog>

class QListWidget;
class QPlainTextEdit;
class QCheckBox;
class QSpinBox;
class QLineEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const IndexOptions &options, QWidget *parent = nullptr);

    IndexOptions options() const;

private slots:
    void addPath();
    void removeSelected();

private:
    QListWidget *m_list = nullptr;
    QPlainTextEdit *m_excludeEdit = nullptr;
    QCheckBox *m_skipHidden = nullptr;
    QCheckBox *m_followSymlinks = nullptr;
    QSpinBox *m_maxResults = nullptr;
    QCheckBox *m_watchFs = nullptr;
    QCheckBox *m_minimizeTray = nullptr;
    QCheckBox *m_startInTray = nullptr;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_pinyin = nullptr;
    QLineEdit *m_hotkeyEdit = nullptr;
    IndexOptions m_original;
};
