#include <QSignalSpy>
#include <QTest>

#include "core/CounterEngine.h"
#include "core/KeyCatalog.h"

using namespace kc;

class TestCounterEngine : public QObject
{
    Q_OBJECT
private slots:
    void autoRepeatIsIgnored();
    void countsPresses();
    void correctionAttribution();
    void pauseStopsCounting();
    void flushEmitsDelta();
};

void TestCounterEngine::autoRepeatIsIgnored()
{
    CounterEngine engine;
    QVERIFY(!engine.isPaused());

    engine.onRawKey(pk::A, true);
    engine.onRawKey(pk::A, true);  // 长按自动重复
    engine.onRawKey(pk::A, true);
    QCOMPARE(engine.todayPress(), quint64(1));

    engine.onRawKey(pk::A, false);  // 抬起
    engine.onRawKey(pk::A, true);   // 再次按下
    QCOMPARE(engine.todayPress(), quint64(2));
}

void TestCounterEngine::countsPresses()
{
    CounterEngine engine;
    const quint16 keys[] = {pk::A, pk::S, pk::D, pk::Space, pk::LeftShift, pk::ArrowUp, pk::Numpad1, pk::Semicolon};
    for (quint16 pid : keys)
    {
        engine.onRawKey(pid, true);
        engine.onRawKey(pid, false);
    }
    QCOMPARE(engine.todayPress(), quint64(8));
    QCOMPARE(engine.todayCorr(), quint64(0));
}

void TestCounterEngine::correctionAttribution()
{
    CounterEngine engine;
    engine.onRawKey(pk::A, true);
    engine.onRawKey(pk::A, false);
    engine.onRawKey(pk::Backspace, true);
    engine.onRawKey(pk::Backspace, false);

    QCOMPARE(engine.todayPress(), quint64(2));  // 退格键本身也计入按下
    QCOMPARE(engine.todayCorr(), quint64(1));   // 归因到 A
}

void TestCounterEngine::pauseStopsCounting()
{
    CounterEngine engine;
    engine.setPaused(true);
    QVERIFY(engine.isPaused());

    engine.onRawKey(pk::A, true);
    engine.onRawKey(pk::A, false);
    QCOMPARE(engine.todayPress(), quint64(0));

    // 暂停期间按住不放的键，恢复后不会补计
    engine.onRawKey(pk::B, true);
    engine.setPaused(false);
    engine.onRawKey(pk::B, false);
    engine.onRawKey(pk::B, true);
    engine.onRawKey(pk::B, false);
    QCOMPARE(engine.todayPress(), quint64(1));
}

void TestCounterEngine::flushEmitsDelta()
{
    CounterEngine engine;
    QVector<DayBatch> batches;
    QObject::connect(&engine, &CounterEngine::batchReady, [&batches](const kc::DayBatch& batch) { batches.append(batch); });

    const int today = static_cast<int>(QDate::currentDate().toJulianDay());
    QCOMPARE(engine.currentDay(), today);

    engine.onRawKey(pk::A, true);
    engine.onRawKey(pk::A, false);
    engine.flush();
    QCOMPARE(batches.size(), qsizetype(1));
    QCOMPARE(batches.first().day, today);
    QCOMPARE(batches.first().press[pk::A], quint32(1));

    // 无新数据时不再移交
    engine.flush();
    QCOMPARE(batches.size(), qsizetype(1));

    // 增量移交后，"今天"的内存值仍然保留（用于显示）
    engine.onRawKey(pk::A, true);
    engine.onRawKey(pk::A, false);
    QCOMPARE(engine.todayPress(), quint64(2));
    engine.flush();
    QCOMPARE(batches.size(), qsizetype(2));
    QCOMPARE(batches.last().press[pk::A], quint32(1));  // 第二批只含增量
}

QTEST_GUILESS_MAIN(TestCounterEngine)

#include "tst_engine.moc"
