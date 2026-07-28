#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // Example: "Ctrl+Alt+Space", "Meta+F"
    bool setShortcut(const QString &shortcut);
    void clear();
    QString shortcut() const { return m_shortcut; }
    bool isActive() const { return m_active; }

signals:
    void activated();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    bool parseShortcut(const QString &shortcut, int *mods, int *key) const;
    bool registerNative();
    void unregisterNative();

    QString m_shortcut;
    bool m_active = false;
#ifdef Q_OS_WIN
    int m_id = 1;
    UINT m_winMods = 0;
    UINT m_winVk = 0;
#elif defined(Q_OS_LINUX)
    int m_xmods = 0;
    int m_xk = 0;
    bool m_x11 = false;
#endif
};
