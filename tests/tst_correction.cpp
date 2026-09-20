#include <QTest>

#include "core/CorrectionTracker.h"

using namespace kc;

class TestCorrectionTracker : public QObject
{
    Q_OBJECT
private slots:
    void attributesBackspace();
    void backspaceWithoutVictim();
    void modifierKeepsContext();
    void navigationBreaksContext();
    void timeoutExpiresContext();
    void capacityKeepsRecent();
};

void TestCorrectionTracker::attributesBackspace()
{
    CorrectionTracker tracker;
    QCOMPARE(tracker.registerKeyDown(pk::A, 1000), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 1200), quint16(pk::A));

    // 连续输入多个字符后，退格按 LIFO 归因
    tracker.reset();
    QCOMPARE(tracker.registerKeyDown(pk::H, 2000), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::E, 2100), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::L, 2200), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 2300), quint16(pk::L));
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 2400), quint16(pk::E));
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 2500), quint16(pk::H));
}

void TestCorrectionTracker::backspaceWithoutVictim()
{
    CorrectionTracker tracker;
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 1000), kInvalidPid);

    // 删完了再删：没有可归因对象
    QCOMPARE(tracker.registerKeyDown(pk::A, 1100), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 1200), quint16(pk::A));
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 1300), kInvalidPid);
}

void TestCorrectionTracker::modifierKeepsContext()
{
    CorrectionTracker tracker;
    QCOMPARE(tracker.registerKeyDown(pk::A, 1000), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::LeftShift, 1010), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::LeftCtrl, 1020), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::CapsLock, 1030), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 1100), quint16(pk::A));
}

void TestCorrectionTracker::navigationBreaksContext()
{
    CorrectionTracker tracker;
    QCOMPARE(tracker.registerKeyDown(pk::A, 1000), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Enter, 1100), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 1200), kInvalidPid);

    QCOMPARE(tracker.registerKeyDown(pk::B, 1300), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::ArrowLeft, 1400), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 1500), kInvalidPid);
}

void TestCorrectionTracker::timeoutExpiresContext()
{
    CorrectionTracker tracker(10000);
    QCOMPARE(tracker.registerKeyDown(pk::A, 1000), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 11000), quint16(pk::A));

    QCOMPARE(tracker.registerKeyDown(pk::B, 12000), kInvalidPid);
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 30000), kInvalidPid);  // 超窗
}

void TestCorrectionTracker::capacityKeepsRecent()
{
    CorrectionTracker tracker;
    for (int i = 0; i < 40; ++i)
    {
        tracker.registerKeyDown(static_cast<quint16>(pk::A + (i % 26)), static_cast<qint64>(1000 + i));
    }
    // 只保留最近 kCapacity 个：第 40 个是 i=39 → 'A' + (39 % 26) = 'N'
    const quint16 expected = static_cast<quint16>(pk::A + (39 % 26));
    QCOMPARE(tracker.registerKeyDown(pk::Backspace, 2000), expected);
}

QTEST_GUILESS_MAIN(TestCorrectionTracker)

#include "tst_correction.moc"
