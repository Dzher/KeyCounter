// keyCounter - 程序入口
#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QTextStream>

#include <algorithm>
#include <cstdio>

#include "app/Application.h"
#include "app/SingleInstance.h"
#include "core/KeyCatalog.h"
#include "core/Periods.h"
#include "core/StatsQuery.h"
#include "core/StatsStore.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{

constexpr const char* kVersion = "1.0.0";

/// GUI 子系统程序也能在控制台里输出（keyCounter --dump）
void attachConsole()
{
#ifdef Q_OS_WIN
    // 输出已被父进程重定向（管道/文件）时直接用，不要用自己的控制台把它顶掉
    const HANDLE existing = GetStdHandle(STD_OUTPUT_HANDLE);
    if (existing != nullptr && existing != INVALID_HANDLE_VALUE)
    {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS) != FALSE)
    {
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        SetConsoleOutputCP(CP_UTF8);
    }
#endif
}

QString singleInstanceKey()
{
    const QString user = qEnvironmentVariable("USERNAME", QStringLiteral("default"));
    return QStringLiteral("keyCounter-single-%1").arg(user);
}

int runDump()
{
    attachConsole();
    QTextStream out(stdout);

    const QString path = kc::StatsStore::defaultDatabasePath();
    if (!QFileInfo::exists(path))
    {
        out << QStringLiteral("尚未生成数据文件：%1\n").arg(path);
        return 1;
    }

    kc::StatsQuery query;
    QString error;
    if (!query.open(path, &error))
    {
        out << QStringLiteral("打开数据库失败：%1\n").arg(error);
        return 1;
    }

    out << QStringLiteral("数据文件：%1\n\n").arg(path);
    const QDate today = QDate::currentDate();

    const auto dumpRange = [&](kc::PeriodKind kind)
    {
        const kc::Range range = kc::makeRange(kind, today, 1, 0);
        const kc::PeriodStats stats = query.query(range);
        out << QStringLiteral("[%1] 键击 %2 · 打错修正 %3 · 活跃 %4 天\n")
                   .arg(kc::periodName(kind))
                   .arg(stats.totalPress)
                   .arg(stats.totalCorr)
                   .arg(stats.activeDays);

        QVector<QPair<quint16, kc::KeyStat>> items;
        for (auto it = stats.keys.constBegin(); it != stats.keys.constEnd(); ++it)
        {
            items.append({it.key(), it.value()});
        }
        std::sort(items.begin(), items.end(),
                  [](const QPair<quint16, kc::KeyStat>& a, const QPair<quint16, kc::KeyStat>& b)
                  { return a.second.press > b.second.press; });

        for (const auto& item : items.mid(0, 8))
        {
            out << QStringLiteral("    %1: %2 次%3\n")
                       .arg(QString::fromLatin1(kc::keyInfo(item.first).name))
                       .arg(item.second.press)
                       .arg(item.second.corr > 0 ? QStringLiteral("（打错 %1）").arg(item.second.corr) : QString());
        }
        out << QStringLiteral("\n");
    };

    dumpRange(kc::PeriodKind::Today);
    dumpRange(kc::PeriodKind::Week);
    dumpRange(kc::PeriodKind::Month);
    dumpRange(kc::PeriodKind::Year);
    out.flush();
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("keyCounter"));
    app.setApplicationDisplayName(QStringLiteral("keyCounter"));
    app.setApplicationVersion(QString::fromLatin1(kVersion));
    app.setOrganizationName(QStringLiteral("keyCounter"));
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon());

    const QStringList arguments = app.arguments();
    if (arguments.contains(QStringLiteral("--dump"), Qt::CaseInsensitive))
    {
        return runDump();
    }

    kc::SingleInstance instance(singleInstanceKey());
    if (!instance.tryLock())
    {
        instance.notifyExisting();
        return 0;
    }

    kc::Application core;
    QString error;
    if (!core.initialize(arguments, &error))
    {
        QMessageBox::critical(nullptr, QStringLiteral("keyCounter 启动失败"), error);
        return 1;
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &core, &kc::Application::shutdown);
    QObject::connect(&instance, &kc::SingleInstance::activationRequested, &core, &kc::Application::showPanel);

    return app.exec();
}
