#pragma once

#include <QDialog>

class QTextBrowser;

class HelpDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr);
};
