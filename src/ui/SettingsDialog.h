// keyCounter - 设置对话框
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QPushButton;

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
    /// 数据目录变化后刷新显示（由 Application 在迁移成功后调用）
    void refreshDataPath();

signals:
    void preferencesChanged();
    void hotkeyChanged(bool enabled, const QKeySequence& sequence);
    void clearDataRequested();
    /// @param migrateExisting true = 把当前数据搬到新目录；false = 直接改用新目录已有的数据
    void dataDirectoryChangeRequested(const QString& dir, bool migrateExisting);

private:
    QWidget* buildGeneralTab();
    QWidget* buildHotkeyTab();
    QWidget* buildDataTab();
    QWidget* buildAboutTab();
    void refreshAutostartState();
    void chooseDataDirectory();
    void requestDataDirectory(const QString& dir);

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
    QPushButton* m_resetDataDirButton = nullptr;
};

}  // namespace kc
