// keyCounter - 开机自启（HKCU\...\CurrentVersion\Run）
#pragma once

#include <QString>

namespace kc
{

namespace autostart
{

/// 运行键中登记的命令行："<exe 绝对路径>" --silent
QString commandLine();
QString executablePath();

bool isEnabled();
/// @param error 失败原因（如被安全软件/组策略拦截）
bool setEnabled(bool enabled, QString* error = nullptr);

}  // namespace autostart

}  // namespace kc
