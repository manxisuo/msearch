#include "index/FileEntry.h"
#include "index/IndexDatabase.h"
#include "search/QueryParser.h"
#include "search/SearchEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

class CoreTests : public QObject
{
    Q_OBJECT
private slots:
    void queryParser_extAndExclude();
    void queryParser_sizeRange();
    void indexDatabase_roundTrip();
    void searchEngine_basicMatch();
};

void CoreTests::queryParser_extAndExclude()
{
    const ParsedQuery q = QueryParser::parse(QStringLiteral("report ext:pdf -tmp"));
    QVERIFY(q.valid);
    QCOMPARE(q.extensions, QStringList() << QStringLiteral("pdf"));
    QCOMPARE(q.terms.size(), 2);
    QCOMPARE(q.terms[0].value, QStringLiteral("report"));
    QVERIFY(!q.terms[0].exclude);
    QCOMPARE(q.terms[1].value, QStringLiteral("tmp"));
    QVERIFY(q.terms[1].exclude);
}

void CoreTests::queryParser_sizeRange()
{
    const ParsedQuery q = QueryParser::parse(QStringLiteral("size:1k..2m"));
    QVERIFY(q.valid);
    QCOMPARE(q.minSize, qint64(1024));
    QCOMPARE(q.maxSize, qint64(2) * 1024 * 1024);
}

void CoreTests::indexDatabase_roundTrip()
{
    IndexDatabase db;
    db.setIncludePaths(QStringList() << QStringLiteral("/tmp/demo"));

    FileEntry a;
    a.name = QStringLiteral("hello.txt");
    a.path = QStringLiteral("/tmp/demo");
    a.size = 42;
    a.mtime = 1700000000;
    a.isDir = false;
    db.add(a);

    FileEntry b;
    b.name = QStringLiteral("sub");
    b.path = QStringLiteral("/tmp/demo");
    b.isDir = true;
    b.mtime = 1700000001;
    db.add(b);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.path() + QStringLiteral("/index.msdb");
    QVERIFY(db.saveToFile(file));

    IndexDatabase loaded;
    QVERIFY(loaded.loadFromFile(file));
    QCOMPARE(loaded.count(), 2);
    QCOMPARE(loaded.includePaths(), QStringList() << QStringLiteral("/tmp/demo"));

    const QVector<FileEntry> snap = loaded.snapshot();
    QCOMPARE(snap[0].name, QStringLiteral("hello.txt"));
    QCOMPARE(snap[0].size, qint64(42));
    QVERIFY(snap[1].isDir);
}

void CoreTests::searchEngine_basicMatch()
{
    IndexDatabase db;
    FileEntry a;
    a.name = QStringLiteral("Report-Final.pdf");
    a.path = QStringLiteral("/docs");
    a.size = 2048;
    a.mtime = 1700000000;
    db.add(a);

    FileEntry b;
    b.name = QStringLiteral("notes.txt");
    b.path = QStringLiteral("/docs");
    b.size = 10;
    b.mtime = 1700000000;
    db.add(b);

    SearchEngine engine(&db);
    QVector<FileEntry> hits;
    bool truncated = false;
    QObject::connect(&engine, &SearchEngine::resultsReady,
                     [&](const QVector<FileEntry> &r, const QString &, bool t) {
                         hits = r;
                         truncated = t;
                     });

    engine.search(QStringLiteral("ext:pdf report"), false, int(EntryFilter::All), 100, false);
    // slot runs synchronously when called directly on same thread
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits[0].name, QStringLiteral("Report-Final.pdf"));
    QVERIFY(!truncated);

    engine.search(QStringLiteral("notes -pdf"), false, int(EntryFilter::All), 100, false);
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits[0].name, QStringLiteral("notes.txt"));
}

QTEST_MAIN(CoreTests)
#include "test_core.moc"
