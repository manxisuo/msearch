#include "app/HelpDialog.h"

#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("搜索语法帮助"));
    resize(560, 520);

    auto *browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(false);
    browser->setHtml(
        QStringLiteral(
            "<h3>%1</h3>"
            "<ul>"
            "<li>%2</li>"
            "<li><code>*</code> / <code>?</code>：%3</li>"
            "<li>%4</li>"
            "<li><code>-词</code>：%5</li>"
            "<li>%6</li>"
            "</ul>"
            "<h3>%7</h3>"
            "<ul>"
            "<li><code>name:</code> / <code>n:</code> %8</li>"
            "<li><code>path:</code> / <code>p:</code> %9</li>"
            "<li><code>regex:</code> / <code>r:</code> %10</li>"
            "<li><code>ext:</code> / <code>e:</code> %11</li>"
            "<li><code>size:</code> / <code>s:</code> %12</li>"
            "<li><code>dm:</code> %13</li>"
            "</ul>"
            "<h3>%14</h3>"
            "<p>%15</p>"
            "<h3>%16</h3>"
            "<pre>report\n*.pdf\next:png;jpg size:&gt;500kb\n"
            "path:Documents -tmp\nregex:^IMG_\\d+\ndm:today ext:log</pre>")
            .arg(tr("基础"),
                 tr("普通关键字：匹配文件名子串"),
                 tr("通配符（整名匹配，如 *.pdf）"),
                 tr("多个词用空格：表示 AND；同词内 a|b 表示 OR"),
                 tr("排除包含该词的结果"),
                 tr("引号：\"my file\" 匹配含空格的关键字"),
                 tr("字段"),
                 tr("仅匹配文件名"),
                 tr("匹配路径"),
                 tr("正则匹配文件名"),
                 tr("扩展名，多个用 ; ，如 ext:pdf;docx"),
                 tr("大小，如 size:>1mb、size:100k..10m"),
                 tr("修改时间，如 dm:today、dm:week、dm:>2024-01-01"),
                 tr("拼音"),
                 tr("纯字母关键字会同时尝试拼音首字母匹配中文文件名（近似算法）。"),
                 tr("示例")));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *root = new QVBoxLayout(this);
    root->addWidget(browser, 1);
    root->addWidget(buttons);
}
