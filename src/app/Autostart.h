#pragma once

#include <QString>

namespace Autostart {

bool isEnabled();
bool setEnabled(bool enabled, const QString &executablePath);

} // namespace Autostart
