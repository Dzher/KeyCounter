// keyCounter - 设置对话框
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;

namespace kc
{

class CounterEngine;
class Settings;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(Settings& settings, CounterEngine& engine, QWidget* parent = nullptr);

    /// 全局快捷键是否注册成功（由 Application 回填）
    void setHotkeyStatus(bool ok, const QString& message);

signals:
    void preferencesChanged();
    void hotkeyChanged(bool enabled, const QKeySequence& sequence);
    void clearDataRequested();

private:
    QWidget* buildGeneralTab();
    QWidget* buildHotkeyTab();
    QWidget* buildDataTab();
    QWidget* buildAboutTab();
    void refreshAutostartState();

    Settings& m_settings;
    CounterEngine& m_engine;

    QCheckBox* m_autostart = nullptr;
    QLabel* m_autostartHint = nullptr;
    QComboBox* m_theme = nullptr;
    QComboBox* m_ramp = nullptr;
    QComboBox* m_layout = nullptr;
    QComboBox* m_weekStart = nullptr;
    QCheckBox* m_reduceMotion = nullptr;
    QCheckBox* m_hourlyChart = nullptr;

    QCheckBox* m_hotkeyEnabled = nullptr;
    QKeySequenceEdit* m_hotkeyEdit = nullptr;
    QLabel* m_hotkeyStatus = nullptr;

    QComboBox* m_errorMode = nullptr;
    QLabel* m_dataPath = nullptr;
};

}  // namespace kc
