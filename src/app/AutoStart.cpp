// keyCounter - 开机自启实现
#include "AutoStart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace kc
{
namespace autostart
{
namespace
{

const QString kRunKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString kValueName = QStringLiteral("keyCounter");

}  // namespace

QString executablePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

QString commandLine()
{
    return QStringLiteral("\"%1\" --silent").arg(executablePath());
}

bool isEnabled()
{
    QSettings run(kRunKey, QSettings::NativeFormat);
    const QString current = run.value(kValueName).toString().trimmed();
    if (current.isEmpty())
    {
        return false;
    }
    // 只看是否登记了自己（忽略路径差异，路径漂移由 setEnabled 自愈）
    return current.contains(QCoreApplication::applicationName(), Qt::CaseInsensitive) ||
           current.contains(QStringLiteral("keycounter"), Qt::CaseInsensitive);
}

bool setEnabled(bool enabled, QString* error)
{
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (enabled)
    {
        run.setValue(kValueName, commandLine());
        run.sync();
        if (run.status() != QSettings::NoError)
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("写入注册表失败（可能被安全软件或组策略拦截），可手动添加：%1 = %2")
                             .arg(kValueName, commandLine());
            }
            return false;
        }
        return true;
    }

    run.remove(kValueName);
    run.sync();
    if (run.status() != QSettings::NoError && error != nullptr)
    {
        *error = QStringLiteral("删除注册表项失败");
        return false;
    }
    return true;
}

}  // namespace autostart

}  // namespace kc
