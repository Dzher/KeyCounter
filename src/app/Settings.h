// keyCounter - 用户设置（HKCU\Software\keyCounter）
#pragma once

#include <QByteArray>
#include <QKeySequence>
#include <QObject>
#include <QSettings>
#include <QString>

class QDir;

namespace kc
{

class Settings : public QObject
{
    Q_OBJECT
public:
    enum class ThemeMode
    {
        System = 0,
        Dark = 1,
        Light = 2,
    };

    enum class ErrorMode
    {
        Attributed = 0,  ///< 归因口径：哪个键最常被打错
        Literal = 1,     ///< 字面口径：删除键按了多少次
    };

    explicit Settings(QObject* parent = nullptr);

    /// 开机自启：始终以注册表真实状态为准
    bool autostart() const;
    bool setAutostart(bool enabled, QString* error = nullptr);

    ThemeMode themeMode() const;
    void setThemeMode(ThemeMode mode);

    int heatRamp() const;
    void setHeatRamp(int kind);

    QString layoutId() const;
    void setLayoutId(const QString& id);

    int weekStart() const;  ///< 1 = 周一，7 = 周日
    void setWeekStart(int value);

    bool reduceMotion() const;
    void setReduceMotion(bool value);

    ErrorMode errorMode() const;
    void setErrorMode(ErrorMode mode);

    /// 是否在趋势区显示时段分布子标签
    bool hourlyChart() const;
    void setHourlyChart(bool value);

    bool backupEnabled() const;
    void setBackupEnabled(bool value);

    /// 自定义数据目录；空字符串表示使用默认位置（便携模式或 %APPDATA%/keyCounter）
    QString dataDirectory() const;
    void setDataDirectory(const QString& dir);
    /// 实际生效的数据目录：自定义目录优先，否则回退默认位置
    QString resolvedDataDir() const;
    /// 实际生效的数据库文件路径
    QString databasePath() const;
    bool usesCustomDataDirectory() const;

    bool hotkeyEnabled() const;
    void setHotkeyEnabled(bool value);
    QKeySequence hotkey() const;
    void setHotkey(const QKeySequence& sequence);

    int lastPeriod() const;
    void setLastPeriod(int kind);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& value);

    bool onboarded() const;
    void setOnboarded(bool value);

    void sync();

signals:
    void changed();

private:
    QSettings m_settings;
};

}  // namespace kc
