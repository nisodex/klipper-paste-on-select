/*
    SPDX-FileCopyrightText: 2026 Nuri <nuri@example.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "synthetickeyboard.h"
#include "keymapper.h"
#include "klipper_debug.h"

#include <QGuiApplication>
#include <QKeyCombination>
#include <KWindowSystem>

#if __has_include(<QtWaylandClient/QWaylandClientExtension>)
#include <QtWaylandClient/QWaylandClientExtension>
#include "qwayland-fake-input.h"
#define HAVE_WAYLAND_CLIENT 1
#endif

#ifdef HAVE_X11_XTEST
#include <xcb/xcb.h>
#include <xcb/xtest.h>
#include <X11/Xlib.h>
#endif

#if HAVE_WAYLAND_CLIENT
class FakeInputDevice : public QWaylandClientExtensionTemplate<FakeInputDevice, &QtWayland::org_kde_kwin_fake_input::destroy>,
                        public QtWayland::org_kde_kwin_fake_input
{
    Q_OBJECT
public:
    explicit FakeInputDevice(QObject *parent = nullptr)
        : QWaylandClientExtensionTemplate<FakeInputDevice, &QtWayland::org_kde_kwin_fake_input::destroy>(6, parent)
    {
        connect(this, &QWaylandClientExtension::activeChanged, this, [this]() {
            tryAuthenticate();
        });
        initialize();
        tryAuthenticate();
    }

private:
    void tryAuthenticate()
    {
        if (isActive() && !m_authenticated) {
            m_authenticated = true;
            qCDebug(KLIPPER_LOG) << "FakeInputDevice active, authenticating...";
            authenticate(QStringLiteral("klipper"), QStringLiteral("Paste on select"));
        }
    }

    bool m_authenticated = false;
};
#endif

class SyntheticKeyboard::Private
{
public:
#if HAVE_WAYLAND_CLIENT
    std::unique_ptr<FakeInputDevice> fakeInput;
#endif
};

SyntheticKeyboard *SyntheticKeyboard::self()
{
    static SyntheticKeyboard instance;
    return &instance;
}

SyntheticKeyboard::SyntheticKeyboard(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
#if HAVE_WAYLAND_CLIENT
    if (KWindowSystem::isPlatformWayland()) {
        d->fakeInput = std::make_unique<FakeInputDevice>(this);
    }
#endif

    if (qApp) {
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
#if HAVE_WAYLAND_CLIENT
            d->fakeInput.reset();
#endif
        });
    }
}

SyntheticKeyboard::~SyntheticKeyboard()
{
#if HAVE_WAYLAND_CLIENT
    d->fakeInput.reset();
#endif
}

void SyntheticKeyboard::sendKeySequence(const QKeySequence &seq)
{
    if (seq.isEmpty() || seq.count() == 0) {
        qCDebug(KLIPPER_LOG) << "Empty key sequence provided to SyntheticKeyboard";
        return;
    }

    if (KWindowSystem::isPlatformWayland()) {
#if HAVE_WAYLAND_CLIENT
        if (!d->fakeInput || !d->fakeInput->isActive()) {
            qCWarning(KLIPPER_LOG) << "Wayland fake_input interface is not available or not active";
            return;
        }

        QList<quint32> modifiers;
        quint32 key = 0;
        if (!KeyMapper::decomposeKeySequence(seq, modifiers, key)) {
            qCWarning(KLIPPER_LOG) << "Failed to decompose key sequence for Wayland input" << seq;
            return;
        }

        // 1. Press all modifiers
        for (quint32 mod : modifiers) {
            d->fakeInput->keyboard_key(mod, 1);
        }

        // 2. Press and release the primary key
        d->fakeInput->keyboard_key(key, 1);
        d->fakeInput->keyboard_key(key, 0);

        // 3. Release all modifiers in reverse order (guaranteed)
        for (auto it = modifiers.crbegin(); it != modifiers.crend(); ++it) {
            d->fakeInput->keyboard_key(*it, 0);
        }
#else
        qCWarning(KLIPPER_LOG) << "Compiled without Wayland client fake-input support";
#endif
        return;
    }

#if HAVE_X11_XTEST
    if (KWindowSystem::isPlatformX11()) {
        auto *x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        if (!x11App) {
            return;
        }
        xcb_connection_t *conn = x11App->connection();
        Display *display = x11App->display();
        if (!conn || !display) {
            return;
        }

        const QKeyCombination comb = seq[0];
        const Qt::Key key = comb.key();
        const quint32 keySym = KeyMapper::toX11KeySym(key);
        if (keySym == 0) {
            qCWarning(KLIPPER_LOG) << "Could not map key to X11 keysym" << key;
            return;
        }

        const xcb_keycode_t keyCode = XKeysymToKeycode(display, keySym);
        if (keyCode == 0) {
            qCWarning(KLIPPER_LOG) << "Could not resolve X11 keycode for keysym" << keySym;
            return;
        }

        const QList<quint32> modSyms = KeyMapper::modifiersToX11KeySyms(comb.keyboardModifiers());
        QList<xcb_keycode_t> modCodes;
        modCodes.reserve(modSyms.size());
        for (quint32 sym : modSyms) {
            if (xcb_keycode_t mc = XKeysymToKeycode(display, sym)) {
                modCodes.append(mc);
            }
        }

        // 1. Press modifiers
        for (xcb_keycode_t mc : modCodes) {
            xcb_test_fake_input(conn, XCB_KEY_PRESS, mc, XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        }

        // 2. Press and release key
        xcb_test_fake_input(conn, XCB_KEY_PRESS, keyCode, XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        xcb_test_fake_input(conn, XCB_KEY_RELEASE, keyCode, XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);

        // 3. Release modifiers in reverse order
        for (auto it = modCodes.crbegin(); it != modCodes.crend(); ++it) {
            xcb_test_fake_input(conn, XCB_KEY_RELEASE, *it, XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        }

        xcb_flush(conn);
        return;
    }
#endif
}

#include "synthetickeyboard.moc"
