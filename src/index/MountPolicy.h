#pragma once

#include <QString>

namespace MountPolicy {

struct Options {
    bool skipNetwork = true;
    bool skipReadOnly = false;
};

// Linux: inspect /proc/mounts + statvfs. Other OS: always false.
bool isNetworkPath(const QString &absolutePath);
bool isReadOnlyPath(const QString &absolutePath);

bool shouldSkipPath(const QString &absolutePath, const Options &opt);

} // namespace MountPolicy
