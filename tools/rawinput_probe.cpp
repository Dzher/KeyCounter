// keyCounter - 采集探针：验证 Raw Input 是否真的收到按键（开发/诊断用）
// 用法： keyCounterRawProbe [运行秒数]
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>
#include <QTimer>

#include "core/KeyCatalog.h"
#include "core/RawInputMonitor.h"

using namespace kc;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    int seconds = 20;
    if (argc > 1)
    {
        bool ok = false;
        const int parsed = QString::fromLocal8Bit(argv[1]).toInt(&ok);
        if (ok && parsed > 0)
        {
            seconds = parsed;
        }
    }

    RawInputMonitor monitor;
    QString error;
    const bool started = monitor.start(&error);
    out << QStringLiteral("RawInputMonitor::start -> %1 %2\n").arg(started ? QStringLiteral("OK") : QStringLiteral("FAILED"),
                                                                  error);
    out.flush();
    if (!started)
    {
        return 1;
    }

    int count = 0;
    QObject::connect(&monitor, &RawInputMonitor::rawKey, [&out, &count](quint16 pid, bool down, quint32 vk)
                     {
                         ++count;
                         out << QStringLiteral("#%1 pid=0x%2 %3 vk=0x%4 name=%5\n")
                                    .arg(count)
                                    .arg(pid, 3, 16, QLatin1Char('0'))
                                    .arg(down ? QStringLiteral("DOWN") : QStringLiteral("UP  "))
                                    .arg(vk, 2, 16, QLatin1Char('0'))
                                    .arg(QString::fromLatin1(keyInfo(pid).name));
                         out.flush();
                     });
    QObject::connect(&monitor, &RawInputMonitor::sessionLocked, [&out](bool locked)
                     { out << QStringLiteral("[session] locked=%1\n").arg(locked); out.flush(); });
    QObject::connect(&monitor, &RawInputMonitor::systemSuspending, [&out] { out << QStringLiteral("[power] suspending\n"); out.flush(); });

    QTimer::singleShot(seconds * 1000, &app, &QCoreApplication::quit);
    out << QStringLiteral("监听中（%1 秒）…\n").arg(seconds);
    out.flush();

    const int code = app.exec();
    monitor.stop();
    out << QStringLiteral("共捕获 %1 个事件\n").arg(count);
    out.flush();
    return code;
}
