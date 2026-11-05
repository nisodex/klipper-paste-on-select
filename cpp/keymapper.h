/*
    SPDX-FileCopyrightText: 2026 Nuri <nuri@example.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KEYMAPPER_H
#define KEYMAPPER_H

#if __has_include("klipper_export.h")
#include "klipper_export.h"
#endif

#ifndef KLIPPER_EXPORT
#define KLIPPER_EXPORT
#endif

#include <QtGlobal>
#include <Qt>
#include <QList>

class QKeySequence;

namespace KeyMapper
{
    KLIPPER_EXPORT quint32 toLinuxKey(Qt::Key key);
    KLIPPER_EXPORT QList<quint32> modifiersToLinuxKeys(Qt::KeyboardModifiers modifiers);
    KLIPPER_EXPORT bool decomposeKeySequence(const QKeySequence &seq, QList<quint32> &modifiersOut, quint32 &keyOut);

    KLIPPER_EXPORT quint32 toX11KeySym(Qt::Key key);
    KLIPPER_EXPORT QList<quint32> modifiersToX11KeySyms(Qt::KeyboardModifiers modifiers);

    KLIPPER_EXPORT QKeySequence keySequenceFromString(const QString &str);
}

#endif // KEYMAPPER_H
