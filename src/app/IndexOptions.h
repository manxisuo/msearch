#pragma once

#include <QString>
#include <QStringList>

struct IndexOptions {
    QStringList includePaths;
    QStringList excludePatterns;
    bool skipHidden = false;
    bool followSymlinks = false;
    int maxResults = 5000;

    bool watchFilesystem = true;
    bool minimizeToTray = true;
    bool startInTray = false;
    bool autostart = false;
    QString hotkey = QStringLiteral("Ctrl+Alt+Space");
    bool pinyinEnabled = true;
    QStringList bookmarks; // saved queries
};
