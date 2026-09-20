// keyCounter - 用户设置实现
#include "Settings.h"

#include "AutoStart.h"

namespace kc
{
namespace
{
const QString kRegistryRoot = QStringLiteral("HKEY_CURRENT_USER\\Software\\keyCounter");
}

Settings::Settings(QObject* parent) : QObject(parent), m_settings(kRegistryRoot, QSettings::NativeFormat) {}

bool Settings::autostart() const
{
    return autostart::isEnabled();
}

bool Settings::setAutostart(bool enabled, QString* error)
{
    const bool ok = autostart::setEnabled(enabled, error);
    if (ok)
    {
        emit changed();
    }
    return ok;
}

Settings::ThemeMode Settings::themeMode() const
{
    const int value = m_settings.value(QStringLiteral("general/theme"), 0).toInt();
    return static_cast<ThemeMode>(qBound(0, value, 2));
}

void Settings::setThemeMode(ThemeMode mode)
{
    m_settings.setValue(QStringLiteral("general/theme"), static_cast<int>(mode));
    emit changed();
}

int Settings::heatRamp() const
{
    return qBound(0, m_settings.value(QStringLiteral("general/heatRamp"), 0).toInt(), 2);
}

void Settings::setHeatRamp(int kind)
{
    m_settings.setValue(QStringLiteral("general/heatRamp"), kind);
    emit changed();
}

QString Settings::layoutId() const
{
    return m_settings.value(QStringLiteral("general/layout"), QStringLiteral("ansi-104")).toString();
}

void Settings::setLayoutId(const QString& id)
{
    m_settings.setValue(QStringLiteral("general/layout"), id);
    emit changed();
}

int Settings::weekStart() const
{
    const int value = m_settings.value(QStringLiteral("general/weekStart"), 1).toInt();
    return (value == 7) ? 7 : 1;
}

void Settings::setWeekStart(int value)
{
    m_settings.setValue(QStringLiteral("general/weekStart"), value == 7 ? 7 : 1);
    emit changed();
}

bool Settings::reduceMotion() const
{
    return m_settings.value(QStringLiteral("general/reduceMotion"), false).toBool();
}

void Settings::setReduceMotion(bool value)
{
    m_settings.setValue(QStringLiteral("general/reduceMotion"), value);
    emit changed();
}

Settings::ErrorMode Settings::errorMode() const
{
    return static_cast<ErrorMode>(qBound(0, m_settings.value(QStringLiteral("stats/errorMode"), 0).toInt(), 1));
}

void Settings::setErrorMode(ErrorMode mode)
{
    m_settings.setValue(QStringLiteral("stats/errorMode"), static_cast<int>(mode));
    emit changed();
}

bool Settings::hourlyChart() const
{
    return m_settings.value(QStringLiteral("ui/hourlyChart"), true).toBool();
}

void Settings::setHourlyChart(bool value)
{
    m_settings.setValue(QStringLiteral("ui/hourlyChart"), value);
    emit changed();
}

bool Settings::backupEnabled() const
{
    return m_settings.value(QStringLiteral("data/backup"), true).toBool();
}

void Settings::setBackupEnabled(bool value)
{
    m_settings.setValue(QStringLiteral("data/backup"), value);
    emit changed();
}

bool Settings::hotkeyEnabled() const
{
    return m_settings.value(QStringLiteral("general/hotkeyEnabled"), false).toBool();
}

void Settings::setHotkeyEnabled(bool value)
{
    m_settings.setValue(QStringLiteral("general/hotkeyEnabled"), value);
    emit changed();
}

QKeySequence Settings::hotkey() const
{
    const QString text = m_settings.value(QStringLiteral("general/hotkeySequence"), QStringLiteral("Ctrl+Alt+K")).toString();
    return QKeySequence::fromString(text, QKeySequence::PortableText);
}

void Settings::setHotkey(const QKeySequence& sequence)
{
    m_settings.setValue(QStringLiteral("general/hotkeySequence"),
                        sequence.toString(QKeySequence::PortableText));
    emit changed();
}

int Settings::lastPeriod() const
{
    return qBound(0, m_settings.value(QStringLiteral("ui/lastPeriod"), 0).toInt(), 4);
}

void Settings::setLastPeriod(int kind)
{
    m_settings.setValue(QStringLiteral("ui/lastPeriod"), kind);
}

QByteArray Settings::windowGeometry() const
{
    return m_settings.value(QStringLiteral("ui/geometry")).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& value)
{
    m_settings.setValue(QStringLiteral("ui/geometry"), value);
}

bool Settings::onboarded() const
{
    return m_settings.value(QStringLiteral("ui/onboarded"), false).toBool();
}

void Settings::setOnboarded(bool value)
{
    m_settings.setValue(QStringLiteral("ui/onboarded"), value);
}

void Settings::sync()
{
    m_settings.sync();
}

}  // namespace kc
