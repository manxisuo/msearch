#include "app/Autostart.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <string>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace Autostart {

#ifdef Q_OS_LINUX
static QString desktopFilePath()
{
    const QString dir = QDir::homePath() + QStringLiteral("/.config/autostart");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/msearch.desktop");
}
#endif

bool isEnabled()
{
#ifdef Q_OS_LINUX
    return QFile::exists(desktopFilePath());
#elif defined(Q_OS_WIN)
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t buf[1024];
    DWORD type = 0;
    DWORD size = sizeof(buf);
    const LONG ok = RegQueryValueExW(key, L"MSearch", nullptr, &type,
                                     reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(key);
    return ok == ERROR_SUCCESS;
#else
    return false;
#endif
}

bool setEnabled(bool enabled, const QString &executablePath)
{
#ifdef Q_OS_LINUX
    const QString path = desktopFilePath();
    if (!enabled) {
        QFile::remove(path);
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << QStringLiteral("[Desktop Entry]\n");
    out << QStringLiteral("Type=Application\n");
    out << QStringLiteral("Name=MSearch\n");
    out << QStringLiteral("Comment=Fast filename search\n");
    out << QStringLiteral("Exec=\"%1\"\n").arg(executablePath);
    out << QStringLiteral("Icon=system-search\n");
    out << QStringLiteral("Terminal=false\n");
    out << QStringLiteral("X-GNOME-Autostart-enabled=true\n");
    return true;

#elif defined(Q_OS_WIN)
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE | KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    if (!enabled) {
        RegDeleteValueW(key, L"MSearch");
        RegCloseKey(key);
        return true;
    }

    const std::wstring value = (QStringLiteral("\"") + executablePath + QLatin1Char('"')).toStdWString();
    const LONG ok = RegSetValueExW(key, L"MSearch", 0, REG_SZ,
                                   reinterpret_cast<const BYTE *>(value.c_str()),
                                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return ok == ERROR_SUCCESS;
#else
    Q_UNUSED(enabled);
    Q_UNUSED(executablePath);
    return false;
#endif
}

} // namespace Autostart
