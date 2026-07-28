#pragma once

#include <QStringList>

struct IndexOptions {
    QStringList includePaths;
    QStringList excludePatterns; // 名称或路径通配，如 *.o、*/.cache/*
    bool skipHidden = false;
    bool followSymlinks = false;
    int maxResults = 5000;
};
