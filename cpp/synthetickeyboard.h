/*
    SPDX-FileCopyrightText: 2026 Nuri <nuri@example.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SYNTHETICKEYBOARD_H
#define SYNTHETICKEYBOARD_H

#include <QKeySequence>
#include <QObject>
#include <memory>
#include "klipper_export.h"

class KLIPPER_EXPORT SyntheticKeyboard : public QObject
{
    Q_OBJECT

public:
    static SyntheticKeyboard *self();
    ~SyntheticKeyboard() override;

    void sendKeySequence(const QKeySequence &seq);

private:
    explicit SyntheticKeyboard(QObject *parent = nullptr);

    class Private;
    std::unique_ptr<Private> d;
};

#endif // SYNTHETICKEYBOARD_H
