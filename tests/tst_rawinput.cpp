// 验证 RAWKEYBOARD -> 物理键 id 的解析与伪事件过滤（不依赖真实硬件）
#include <QTest>

#include "core/KeyCatalog.h"
#include "core/RawInputMonitor.h"

using namespace kc;

class TestRawInputDecode : public QObject
{
    Q_OBJECT
private slots:
    void encodesPhysicalKeys();
    void distinguishesExtendedKeys();
    void filtersPseudoEvents();
    void reportsKeyUp();
};

namespace
{
// RAWKEYBOARD::Flags（windows.h 中的 RI_KEY_*）
constexpr quint16 kBreak = 0x01;
constexpr quint16 kE0 = 0x02;
constexpr quint16 kE1 = 0x04;

RawKeyEvent decodeOrFail(quint8 makeCode, quint16 flags, quint8 vkey, bool expectedOk = true)
{
    RawKeyEvent event;
    const bool ok = decodeRawKeyboard(makeCode, flags, vkey, &event);
    if (ok != expectedOk)
    {
        qWarning("decodeRawKeyboard(make=0x%02X flags=0x%02X vk=0x%02X) -> %d, expected %d", makeCode, flags, vkey, ok,
                 expectedOk);
    }
    Q_ASSERT(ok == expectedOk);
    return event;
}
}  // namespace

void TestRawInputDecode::encodesPhysicalKeys()
{
    const RawKeyEvent a = decodeOrFail(0x1E, 0, 0x41);
    QCOMPARE(a.pid, quint16(0x01E));
    QVERIFY(a.down);
    QCOMPARE(QString::fromLatin1(keyInfo(a.pid).name), QStringLiteral("A"));

    const RawKeyEvent space = decodeOrFail(0x39, 0, 0x20);
    QCOMPARE(space.pid, quint16(0x039));
}

void TestRawInputDecode::distinguishesExtendedKeys()
{
    // 小键盘回车（E0 1C）与主回车（1C）
    QCOMPARE(decodeOrFail(0x1C, 0, 0x0D).pid, quint16(0x01C));
    QCOMPARE(decodeOrFail(0x1C, kE0, 0x0D).pid, quint16(0x11C));

    // 小键盘方向键与独立方向键
    QCOMPARE(decodeOrFail(0x48, 0, 0x38).pid, quint16(0x048));  // 小键盘 8
    QCOMPARE(decodeOrFail(0x48, kE0, 0x26).pid, quint16(0x148));  // ↑

    // Pause（E1 前缀）
    QCOMPARE(decodeOrFail(0x45, kE1, 0x13).pid, quint16(0x245));

    // 左右 Shift 靠扫描码区分，都不带 E0
    QCOMPARE(decodeOrFail(0x2A, 0, 0x10).pid, quint16(0x02A));
    QCOMPARE(decodeOrFail(0x36, 0, 0x10).pid, quint16(0x036));

    // 右 Ctrl / 右 Alt 靠 E0 区分
    QCOMPARE(decodeOrFail(0x1D, 0, 0x11).pid, quint16(0x01D));
    QCOMPARE(decodeOrFail(0x1D, kE0, 0x11).pid, quint16(0x11D));
    QCOMPARE(decodeOrFail(0x38, kE0, 0x12).pid, quint16(0x138));
}

void TestRawInputDecode::filtersPseudoEvents()
{
    // PrintScreen 会产生 E0 2A / E0 36 的伪 Shift
    decodeOrFail(0x2A, kE0, 0x10, false);
    decodeOrFail(0x36, kE0, 0x10, false);
    decodeOrFail(0x2A, kE0 | kBreak, 0x10, false);

    // 无效 VK 与溢出码
    decodeOrFail(0x64, 0, 0xFF, false);
    decodeOrFail(0xFF, 0, 0x00, false);

    // 真实的左右 Shift 不能被误杀
    decodeOrFail(0x2A, 0, 0x10, true);
    decodeOrFail(0x36, kBreak, 0x10, true);
}

void TestRawInputDecode::reportsKeyUp()
{
    QVERIFY(decodeOrFail(0x1E, 0, 0x41).down);
    QVERIFY(!decodeOrFail(0x1E, kBreak, 0x41).down);
    QVERIFY(!decodeOrFail(0x1E, kBreak | kE0, 0x41).down);
}

QTEST_GUILESS_MAIN(TestRawInputDecode)

#include "tst_rawinput.moc"
