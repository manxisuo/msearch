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

    // Qt 5.12: multi-arg QString::arg() supports at most 9 args; also avoid %10+
    // which breaks with sequential .arg() (because %1 matches inside %10).
    const QString html =
        QStringLiteral(
            "<h3>%1</h3>"
            "<ul>"
            "<li>%2</li>"
            "<li><code>*</code> / <code>?</code>：%3</li>"
            "<li>%4</li>"
            "<li><code>-词</code>：%5</li>"
            "<li>%6</li>"
            "</ul>")
            .arg(tr("基础"))
            .arg(tr("普通关键字：匹配文件名子串"))
            .arg(tr("通配符（整名匹配，如 *.pdf）"))
            .arg(tr("多个词用空格：表示 AND；同词内 a|b 表示 OR"))
            .arg(tr("排除包含该词的结果"))
            .arg(tr("引号：\"my file\" 匹配含空格的关键字"))
        + QStringLiteral(
              "<h3>%1</h3>"
              "<ul>"
              "<li><code>name:</code> / <code>n:</code> %2</li>"
              "<li><code>path:</code> / <code>p:</code> %3</li>"
              "<li><code>regex:</code> / <code>r:</code> %4</li>"
              "<li><code>ext:</code> / <code>e:</code> %5</li>"
              "<li><code>size:</code> / <code>s:</code> %6</li>"
              "<li><code>dm:</code> %7</li>"
              "</ul>")
              .arg(tr("字段"))
              .arg(tr("仅匹配文件名"))
              .arg(tr("匹配路径"))
              .arg(tr("正则匹配文件名"))
              .arg(tr("扩展名，多个用 ; ，如 ext:pdf;docx"))
              .arg(tr("大小，如 size:>1mb、size:100k..10m"))
              .arg(tr("修改时间，如 dm:today、dm:week、dm:>2024-01-01"))
        + QStringLiteral(
              "<h3>%1</h3>"
              "<p>%2</p>"
              "<h3>%3</h3>"
              "<pre>report\n*.pdf\next:png;jpg size:&gt;500kb\n"
              "path:Documents -tmp\nregex:^IMG_\\d+\ndm:today ext:log</pre>")
              .arg(tr("拼音"))
              .arg(tr("纯字母关键字会同时尝试拼音首字母匹配中文文件名（近似算法）。"))
              .arg(tr("示例"));

    browser->setHtml(html);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *root = new QVBoxLayout(this);
    root->addWidget(browser, 1);
    root->addWidget(buttons);
}
