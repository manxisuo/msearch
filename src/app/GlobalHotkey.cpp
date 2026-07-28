#include "app/GlobalHotkey.h"

#include <QGuiApplication>
#include <QKeySequence>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#elif defined(Q_OS_LINUX) && defined(MSEARCH_HAVE_X11)
#  include <QX11Info>
#  include <X11/Xlib.h>
#  include <X11/keysym.h>
#  include <xcb/xcb.h>
#endif

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
    if (qApp)
        qApp->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey()
{
    clear();
    if (qApp)
        qApp->removeNativeEventFilter(this);
}

bool GlobalHotkey::parseShortcut(const QString &shortcut, int *mods, int *key) const
{
    const QKeySequence seq(shortcut);
    if (seq.isEmpty())
        return false;

    const int combined = seq[0];
    *mods = 0;
    if (combined & Qt::ControlModifier)
        *mods |= 1;
    if (combined & Qt::AltModifier)
        *mods |= 2;
    if (combined & Qt::ShiftModifier)
        *mods |= 4;
    if (combined & Qt::MetaModifier)
        *mods |= 8;

    *key = combined & ~Qt::KeyboardModifierMask;
    return *key != 0;
}

bool GlobalHotkey::setShortcut(const QString &shortcut)
{
    clear();
    m_shortcut = shortcut.trimmed();
    if (m_shortcut.isEmpty())
        return false;
    return registerNative();
}

void GlobalHotkey::clear()
{
    unregisterNative();
    m_active = false;
}

bool GlobalHotkey::registerNative()
{
    int mods = 0;
    int key = 0;
    if (!parseShortcut(m_shortcut, &mods, &key))
        return false;

#ifdef Q_OS_WIN
    m_winMods = 0;
    if (mods & 1) m_winMods |= MOD_CONTROL;
    if (mods & 2) m_winMods |= MOD_ALT;
    if (mods & 4) m_winMods |= MOD_SHIFT;
    if (mods & 8) m_winMods |= MOD_WIN;

    m_winVk = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        m_winVk = static_cast<UINT>('A' + (key - Qt::Key_A));
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        m_winVk = static_cast<UINT>('0' + (key - Qt::Key_0));
    else if (key == Qt::Key_Space)
        m_winVk = VK_SPACE;
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        m_winVk = static_cast<UINT>(VK_F1 + (key - Qt::Key_F1));
    else
        return false;

    m_active = RegisterHotKey(nullptr, m_id, m_winMods, m_winVk) != 0;
    return m_active;

#elif defined(Q_OS_LINUX) && defined(MSEARCH_HAVE_X11)
    if (!QX11Info::isPlatformX11())
        return false;

    Display *dpy = QX11Info::display();
    if (!dpy)
        return false;

    m_xmods = 0;
    if (mods & 1) m_xmods |= ControlMask;
    if (mods & 2) m_xmods |= Mod1Mask;
    if (mods & 4) m_xmods |= ShiftMask;
    if (mods & 8) m_xmods |= Mod4Mask;

    KeySym sym = NoSymbol;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        sym = XK_a + (key - Qt::Key_A);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        sym = XK_0 + (key - Qt::Key_0);
    else if (key == Qt::Key_Space)
        sym = XK_space;
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        sym = XK_F1 + (key - Qt::Key_F1);
    else
        return false;

    m_xk = XKeysymToKeycode(dpy, sym);
    if (m_xk == 0)
        return false;

    const Window root = DefaultRootWindow(dpy);
    const unsigned ignoreMasks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (unsigned im : ignoreMasks)
        XGrabKey(dpy, m_xk, m_xmods | im, root, True, GrabModeAsync, GrabModeAsync);
    XSync(dpy, False);
    m_x11 = true;
    m_active = true;
    return true;
#else
    Q_UNUSED(mods);
    Q_UNUSED(key);
    return false;
#endif
}

void GlobalHotkey::unregisterNative()
{
#ifdef Q_OS_WIN
    if (m_active)
        UnregisterHotKey(nullptr, m_id);
#elif defined(Q_OS_LINUX) && defined(MSEARCH_HAVE_X11)
    if (m_x11 && m_active) {
        Display *dpy = QX11Info::display();
        if (dpy) {
            const Window root = DefaultRootWindow(dpy);
            const unsigned ignoreMasks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
            for (unsigned im : ignoreMasks)
                XUngrabKey(dpy, m_xk, m_xmods | im, root);
            XSync(dpy, False);
        }
    }
    m_x11 = false;
#endif
    m_active = false;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
{
    Q_UNUSED(result);

#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == m_id) {
        emit activated();
        return true;
    }
#elif defined(Q_OS_LINUX) && defined(MSEARCH_HAVE_X11)
    if (m_active && eventType == "xcb_generic_event_t" && message) {
        const auto *generic = static_cast<const xcb_generic_event_t *>(message);
        const uint8_t type = generic->response_type & ~0x80;
        if (type == XCB_KEY_PRESS) {
            const auto *kev = static_cast<const xcb_key_press_event_t *>(message);
            if (kev->detail == m_xk) {
                const uint16_t relevant = ControlMask | Mod1Mask | ShiftMask | Mod4Mask;
                if ((kev->state & relevant) == (m_xmods & relevant)) {
                    emit activated();
                    return true;
                }
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}
