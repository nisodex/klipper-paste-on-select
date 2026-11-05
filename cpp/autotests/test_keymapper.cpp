/*
    SPDX-FileCopyrightText: 2026 Nuri <nuri@example.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../keymapper.h"

#include <QKeySequence>
#include <QObject>
#include <QTest>
#include <linux/input-event-codes.h>

class KeyMapperTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBasicKeys();
    void testModifiers();
    void testX11Keys();
    void testX11Modifiers();
    void testDecomposeKeySequence();
    void testInvalidOrEmptySequence();
    void testKeySequenceFromString();
};

void KeyMapperTest::testBasicKeys()
{
    QCOMPARE(KeyMapper::toLinuxKey(Qt::Key_V), static_cast<quint32>(KEY_V));
    QCOMPARE(KeyMapper::toLinuxKey(Qt::Key_C), static_cast<quint32>(KEY_C));
    QCOMPARE(KeyMapper::toLinuxKey(Qt::Key_X), static_cast<quint32>(KEY_X));
    QCOMPARE(KeyMapper::toLinuxKey(Qt::Key_Insert), static_cast<quint32>(KEY_INSERT));
    QCOMPARE(KeyMapper::toLinuxKey(Qt::Key_Enter), static_cast<quint32>(KEY_ENTER));
    QCOMPARE(KeyMapper::toLinuxKey(Qt::Key_Return), static_cast<quint32>(KEY_ENTER));
}

void KeyMapperTest::testModifiers()
{
    const auto ctrl = KeyMapper::modifiersToLinuxKeys(Qt::ControlModifier);
    QCOMPARE(ctrl.size(), 1);
    QCOMPARE(ctrl.first(), static_cast<quint32>(KEY_LEFTCTRL));

    const auto ctrlShift = KeyMapper::modifiersToLinuxKeys(Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(ctrlShift.size(), 2);
    QVERIFY(ctrlShift.contains(static_cast<quint32>(KEY_LEFTCTRL)));
    QVERIFY(ctrlShift.contains(static_cast<quint32>(KEY_LEFTSHIFT)));
}

void KeyMapperTest::testX11Keys()
{
    QCOMPARE(KeyMapper::toX11KeySym(Qt::Key_V), static_cast<quint32>(0x0076)); // 'v'
    QCOMPARE(KeyMapper::toX11KeySym(Qt::Key_C), static_cast<quint32>(0x0063)); // 'c'
    QCOMPARE(KeyMapper::toX11KeySym(Qt::Key_Insert), static_cast<quint32>(0xff63)); // XK_Insert
    QCOMPARE(KeyMapper::toX11KeySym(Qt::Key_Return), static_cast<quint32>(0xff0d)); // XK_Return
    QCOMPARE(KeyMapper::toX11KeySym(Qt::Key_Enter), static_cast<quint32>(0xff0d));
}

void KeyMapperTest::testX11Modifiers()
{
    const auto ctrl = KeyMapper::modifiersToX11KeySyms(Qt::ControlModifier);
    QCOMPARE(ctrl.size(), 1);
    QCOMPARE(ctrl.first(), static_cast<quint32>(0xffe3)); // XK_Control_L

    const auto ctrlShift = KeyMapper::modifiersToX11KeySyms(Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(ctrlShift.size(), 2);
    QVERIFY(ctrlShift.contains(static_cast<quint32>(0xffe3)));
    QVERIFY(ctrlShift.contains(static_cast<quint32>(0xffe1))); // XK_Shift_L
}

void KeyMapperTest::testDecomposeKeySequence()
{
    QList<quint32> modifiers;
    quint32 key = 0;

    // Ctrl+V
    QVERIFY(KeyMapper::decomposeKeySequence(QKeySequence(Qt::CTRL | Qt::Key_V), modifiers, key));
    QCOMPARE(key, static_cast<quint32>(KEY_V));
    QCOMPARE(modifiers.size(), 1);
    QCOMPARE(modifiers.first(), static_cast<quint32>(KEY_LEFTCTRL));

    // Ctrl+Shift+V
    modifiers.clear();
    key = 0;
    QVERIFY(KeyMapper::decomposeKeySequence(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), modifiers, key));
    QCOMPARE(key, static_cast<quint32>(KEY_V));
    QCOMPARE(modifiers.size(), 2);
    QVERIFY(modifiers.contains(static_cast<quint32>(KEY_LEFTCTRL)));
    QVERIFY(modifiers.contains(static_cast<quint32>(KEY_LEFTSHIFT)));

    // Shift+Insert
    modifiers.clear();
    key = 0;
    QVERIFY(KeyMapper::decomposeKeySequence(QKeySequence(Qt::SHIFT | Qt::Key_Insert), modifiers, key));
    QCOMPARE(key, static_cast<quint32>(KEY_INSERT));
    QCOMPARE(modifiers.size(), 1);
    QCOMPARE(modifiers.first(), static_cast<quint32>(KEY_LEFTSHIFT));
}

void KeyMapperTest::testInvalidOrEmptySequence()
{
    QList<quint32> modifiers;
    quint32 key = 0;

    QVERIFY(!KeyMapper::decomposeKeySequence(QKeySequence(), modifiers, key));
    QVERIFY(!KeyMapper::decomposeKeySequence(QKeySequence(Qt::Key_unknown), modifiers, key));
}

void KeyMapperTest::testKeySequenceFromString()
{
    // NativeText from KKeySequenceWidget / KDE config
    const QKeySequence seqCtrlVNative = KeyMapper::keySequenceFromString(QStringLiteral("Control+V"));
    QVERIFY(!seqCtrlVNative.isEmpty());
    QCOMPARE(seqCtrlVNative.toString(QKeySequence::PortableText), QStringLiteral("Ctrl+V"));

    // PortableText from default kcfg
    const QKeySequence seqCtrlVPort = KeyMapper::keySequenceFromString(QStringLiteral("Ctrl+V"));
    QVERIFY(!seqCtrlVPort.isEmpty());
    QCOMPARE(seqCtrlVPort.toString(QKeySequence::PortableText), QStringLiteral("Ctrl+V"));

    // Multi-modifier
    const QKeySequence seqCtrlShiftV = KeyMapper::keySequenceFromString(QStringLiteral("Ctrl+Shift+V"));
    QVERIFY(!seqCtrlShiftV.isEmpty());
    QCOMPARE(seqCtrlShiftV.toString(QKeySequence::PortableText), QStringLiteral("Ctrl+Shift+V"));

    // Shift+Insert
    const QKeySequence seqShiftIns = KeyMapper::keySequenceFromString(QStringLiteral("Shift+Insert"));
    QVERIFY(!seqShiftIns.isEmpty());

    // Empty and invalid
    QVERIFY(KeyMapper::keySequenceFromString(QString()).isEmpty());
    QVERIFY(KeyMapper::keySequenceFromString(QStringLiteral("   ")).isEmpty());
    QVERIFY(KeyMapper::keySequenceFromString(QStringLiteral("invalidKey")).isEmpty());
}

QTEST_MAIN(KeyMapperTest)
#include "test_keymapper.moc"
