#include <QTest>

#include "core/KeyCatalog.h"

using namespace kc;

class TestKeyCatalog : public QObject
{
    Q_OBJECT
private slots:
    void pidEncoding();
    void classification();
};

void TestKeyCatalog::pidEncoding()
{
    QCOMPARE(encodePid(0x1C, false, false), quint16(0x01C));  // 主回车
    QCOMPARE(encodePid(0x1C, true, false), quint16(0x11C));   // 小键盘回车
    QCOMPARE(encodePid(0x2A, false, false), quint16(0x02A));  // 左 Shift
    QCOMPARE(encodePid(0x36, false, false), quint16(0x036));  // 右 Shift（扫描码不同，无 E0）
    QCOMPARE(encodePid(0x1D, true, false), quint16(0x11D));   // 右 Ctrl（E0）
    QCOMPARE(encodePid(0x45, false, true), quint16(0x245));   // Pause（E1）

    QVERIFY(!pidIsExtended(pk::LeftShift));
    QVERIFY(!pidIsExtended(pk::RightShift));  // 右 Shift 靠 0x36 区分，而非 E0
    QVERIFY(pidIsExtended(pk::RightCtrl));
    QVERIFY(pidIsE1(pk::Pause));
    QVERIFY(!pidIsE1(pk::NumLock));

    // 小键盘与主键区同扫描码但 E0 不同 → 必须是不同的统计键
    QVERIFY(pk::Enter != pk::NumpadEnter);
    QVERIFY(pk::Numpad8 != pk::ArrowUp);
    QVERIFY(pk::NumpadMultiply != pk::PrintScreen);
    QVERIFY(pk::NumLock != pk::Pause);
}

void TestKeyCatalog::classification()
{
    QVERIFY(isCorrection(pk::Backspace));
    QVERIFY(isCorrection(pk::Delete));
    QVERIFY(!isCorrection(pk::A));

    QVERIFY(isTypeable(pk::A));
    QVERIFY(isTypeable(pk::Num1));
    QVERIFY(isTypeable(pk::Space));
    QVERIFY(isTypeable(pk::Numpad5));
    QVERIFY(isTypeable(pk::Semicolon));
    QVERIFY(!isTypeable(pk::LeftShift));
    QVERIFY(!isTypeable(pk::Enter));

    QVERIFY(isModifier(pk::LeftShift));
    QVERIFY(isModifier(pk::RightShift));
    QVERIFY(isModifier(pk::LeftCtrl));
    QVERIFY(isModifier(pk::LeftWin));
    QVERIFY(isModifier(pk::CapsLock));

    QVERIFY(isBreak(pk::Enter));
    QVERIFY(isBreak(pk::Escape));
    QVERIFY(isBreak(pk::ArrowLeft));
    QVERIFY(isBreak(pk::F5));
    QVERIFY(!isBreak(pk::LeftShift));

    QCOMPARE(QString::fromLatin1(keyInfo(pk::Backspace).name), QStringLiteral("Backspace"));
    QCOMPARE(QString::fromLatin1(keyInfo(pk::RightShift).name), QStringLiteral("Right Shift"));
    QCOMPARE(QString::fromLatin1(keyInfo(600).name), QStringLiteral("Unknown"));
}

QTEST_GUILESS_MAIN(TestKeyCatalog)

#include "tst_keycatalog.moc"
