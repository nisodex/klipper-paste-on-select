/*
    SPDX-FileCopyrightText: 2026 Nuri <nuri@example.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keymapper.h"

#include <QKeyCombination>
#include <QKeySequence>
#include <linux/input-event-codes.h>

namespace KeyMapper
{

quint32 toLinuxKey(Qt::Key key)
{
    // Letters A-Z
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        static constexpr quint32 letterCodes[] = {
            KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
            KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
            KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X,
            KEY_Y, KEY_Z
        };
        return letterCodes[key - Qt::Key_A];
    }

    // Digits 0-9
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        if (key == Qt::Key_0) {
            return KEY_0;
        }
        return KEY_1 + (key - Qt::Key_1);
    }

    // Function keys F1-F12
    if (key >= Qt::Key_F1 && key <= Qt::Key_F10) {
        return KEY_F1 + (key - Qt::Key_F1);
    }
    if (key == Qt::Key_F11) {
        return KEY_F11;
    }
    if (key == Qt::Key_F12) {
        return KEY_F12;
    }

    // Common navigation and editing keys
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return KEY_ENTER;
    case Qt::Key_Insert:
        return KEY_INSERT;
    case Qt::Key_Delete:
        return KEY_DELETE;
    case Qt::Key_Home:
        return KEY_HOME;
    case Qt::Key_End:
        return KEY_END;
    case Qt::Key_PageUp:
        return KEY_PAGEUP;
    case Qt::Key_PageDown:
        return KEY_PAGEDOWN;
    case Qt::Key_Space:
        return KEY_SPACE;
    case Qt::Key_Tab:
        return KEY_TAB;
    case Qt::Key_Backspace:
        return KEY_BACKSPACE;
    case Qt::Key_Escape:
        return KEY_ESC;
    case Qt::Key_Left:
        return KEY_LEFT;
    case Qt::Key_Right:
        return KEY_RIGHT;
    case Qt::Key_Up:
        return KEY_UP;
    case Qt::Key_Down:
        return KEY_DOWN;
    default:
        return 0;
    }
}

QList<quint32> modifiersToLinuxKeys(Qt::KeyboardModifiers modifiers)
{
    QList<quint32> result;
    if (modifiers & Qt::ControlModifier) {
        result.append(KEY_LEFTCTRL);
    }
    if (modifiers & Qt::ShiftModifier) {
        result.append(KEY_LEFTSHIFT);
    }
    if (modifiers & Qt::AltModifier) {
        result.append(KEY_LEFTALT);
    }
    if (modifiers & Qt::MetaModifier) {
        result.append(KEY_LEFTMETA);
    }
    return result;
}

quint32 toX11KeySym(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return 0x0061 + (key - Qt::Key_A); // 'a' .. 'z'
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return 0x0030 + (key - Qt::Key_0); // '0' .. '9'
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return 0xffbe + (key - Qt::Key_F1); // XK_F1 .. XK_F12
    }

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return 0xff0d; // XK_Return
    case Qt::Key_Insert:
        return 0xff63; // XK_Insert
    case Qt::Key_Delete:
        return 0xffff; // XK_Delete
    case Qt::Key_Home:
        return 0xff50; // XK_Home
    case Qt::Key_End:
        return 0xff57; // XK_End
    case Qt::Key_PageUp:
        return 0xff55; // XK_Page_Up
    case Qt::Key_PageDown:
        return 0xff56; // XK_Page_Down
    case Qt::Key_Space:
        return 0x0020; // XK_space
    case Qt::Key_Tab:
        return 0xff09; // XK_Tab
    case Qt::Key_Backspace:
        return 0xff08; // XK_BackSpace
    case Qt::Key_Escape:
        return 0xff1b; // XK_Escape
    case Qt::Key_Left:
        return 0xff51; // XK_Left
    case Qt::Key_Right:
        return 0xff53; // XK_Right
    case Qt::Key_Up:
        return 0xff52; // XK_Up
    case Qt::Key_Down:
        return 0xff54; // XK_Down
    default:
        return 0;
    }
}

QList<quint32> modifiersToX11KeySyms(Qt::KeyboardModifiers modifiers)
{
    QList<quint32> result;
    if (modifiers & Qt::ControlModifier) {
        result.append(0xffe3); // XK_Control_L
    }
    if (modifiers & Qt::ShiftModifier) {
        result.append(0xffe1); // XK_Shift_L
    }
    if (modifiers & Qt::AltModifier) {
        result.append(0xffe9); // XK_Alt_L
    }
    if (modifiers & Qt::MetaModifier) {
        result.append(0xffeb); // XK_Super_L
    }
    return result;
}

bool decomposeKeySequence(const QKeySequence &seq, QList<quint32> &modifiersOut, quint32 &keyOut)
{
    if (seq.isEmpty() || seq.count() == 0) {
        return false;
    }

    const QKeyCombination comb = seq[0];
    const Qt::Key key = comb.key();
    if (key == Qt::Key_unknown || key == 0) {
        return false;
    }

    const quint32 linuxKey = toLinuxKey(key);
    if (linuxKey == 0) {
        return false;
    }

    modifiersOut = modifiersToLinuxKeys(comb.keyboardModifiers());
    keyOut = linuxKey;
    return true;
}

QKeySequence keySequenceFromString(const QString &str)
{
    if (str.trimmed().isEmpty()) {
        return {};
    }
    QKeySequence seq = QKeySequence::fromString(str, QKeySequence::NativeText);
    if (seq.isEmpty() || seq[0].key() == Qt::Key_unknown || seq[0].key() == 0) {
        seq = QKeySequence::fromString(str, QKeySequence::PortableText);
    }
    if (seq.isEmpty() || seq[0].key() == Qt::Key_unknown || seq[0].key() == 0) {
        return {};
    }
    return seq;
}

} // namespace KeyMapper
