// keyCounter - 热路径微基准：只测"从 Raw Input 到内存计数"的成本，不含落盘
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>

#include "core/CounterEngine.h"
#include "core/KeyCatalog.h"

using namespace kc;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    int iterations = 1000000;
    if (argc > 1)
    {
        bool ok = false;
        const int parsed = QString::fromLocal8Bit(argv[1]).toInt(&ok);
        if (ok && parsed > 0)
        {
            iterations = parsed;
        }
    }

    static const quint16 keys[] = {pk::A,     pk::S,    pk::D,     pk::F,          pk::Space,
                                   pk::E,     pk::Backspace, pk::LeftShift, pk::Enter, pk::Numpad1};
    constexpr int keyCount = sizeof(keys) / sizeof(keys[0]);

    CounterEngine engine;

    // 预热
    for (int i = 0; i < 10000; ++i)
    {
        engine.onRawKey(keys[i % keyCount], true);
        engine.onRawKey(keys[i % keyCount], false);
    }

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iterations; ++i)
    {
        const quint16 pid = keys[i % keyCount];
        engine.onRawKey(pid, true);
        engine.onRawKey(pid, false);
    }
    const qint64 ns = timer.nsecsElapsed();

    const double perKey = static_cast<double>(ns) / static_cast<double>(iterations);
    const double perEvent = perKey / 2.0;

    out << QStringLiteral("iterations (按键次数) : %1\n").arg(iterations);
    out << QStringLiteral("elapsed                : %1 ms\n").arg(ns / 1000000.0, 0, 'f', 2);
    out << QStringLiteral("per key press          : %1 ns\n").arg(perKey, 0, 'f', 1);
    out << QStringLiteral("per raw event (下/上)  : %1 ns\n").arg(perEvent, 0, 'f', 1);
    out << QStringLiteral("budget                 : < 2000 ns/key press  →  %1\n")
               .arg(perKey < 2000.0 ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
    out.flush();

    return perKey < 2000.0 ? 0 : 1;
}
