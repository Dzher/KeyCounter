// keyCounter - 设置对话框实现
#include "SettingsDialog.h"

#include <QApplication>
#include <QSignalBlocker>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "app/Settings.h"
#include "core/CounterEngine.h"
#include "core/StatsStore.h"
#include "layout/LayoutModel.h"
#include "ui/HeatRamp.h"
#include "ui/Theme.h"

namespace kc
{
namespace
{

/// 色带预览（局部小部件，无需 moc）
class RampPreview : public QWidget
{
public:
    RampPreview(int kind, QWidget* parent = nullptr) : QWidget(parent), m_kind(kind)
    {
        setFixedHeight(18);
        setMinimumWidth(180);
    }
    void setKind(int kind)
    {
        m_kind = kind;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QLinearGradient gradient(0, 0, width(), 0);
        for (const auto& stop : HeatRamp::stops(m_kind))
        {
            gradient.setColorAt(qBound(0.0, stop.first, 1.0), stop.second);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
    }

private:
    int m_kind;
};

QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setFont(Theme::instance().font(11));
    label->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::instance().palette().textFaint.name()));
    return label;
}

}  // namespace

SettingsDialog::SettingsDialog(Settings& settings, CounterEngine& engine, QWidget* parent)
    : QDialog(parent), m_settings(settings), m_engine(engine)
{
    setWindowTitle(QStringLiteral("keyCounter 设置"));
    setModal(false);
    resize(560, 520);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);

    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    tabs->addTab(buildGeneralTab(), QStringLiteral("常规"));
    tabs->addTab(buildHotkeyTab(), QStringLiteral("快捷键"));
    tabs->addTab(buildDataTab(), QStringLiteral("统计与数据"));
    tabs->addTab(buildAboutTab(), QStringLiteral("关于"));

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    auto* closeButton = new QPushButton(QStringLiteral("关闭"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    buttonRow->addWidget(closeButton);
    root->addLayout(buttonRow);

    Theme::instance().applyToWidget(this);
}

QWidget* SettingsDialog::buildGeneralTab()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* startupGroup = new QGroupBox(QStringLiteral("启动"), page);
    auto* startupLayout = new QVBoxLayout(startupGroup);
    m_autostart = new QCheckBox(QStringLiteral("开机静默启动（登录后自动在托盘运行）"), startupGroup);
    m_autostart->setChecked(m_settings.autostart());
    connect(m_autostart, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                QString error;
                if (!m_settings.setAutostart(checked, &error))
                {
                    QMessageBox::warning(this, QStringLiteral("设置失败"), error);
                }
                refreshAutostartState();
            });
    startupLayout->addWidget(m_autostart);
    m_autostartHint = hintLabel(QString(), startupGroup);
    startupLayout->addWidget(m_autostartHint);
    refreshAutostartState();
    layout->addWidget(startupGroup);

    auto* appearance = new QGroupBox(QStringLiteral("外观"), page);
    auto* form = new QFormLayout(appearance);

    m_theme = new QComboBox(appearance);
    m_theme->addItems({QStringLiteral("跟随系统"), QStringLiteral("深色"), QStringLiteral("浅色")});
    m_theme->setCurrentIndex(static_cast<int>(m_settings.themeMode()));
    connect(m_theme, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                m_settings.setThemeMode(static_cast<Settings::ThemeMode>(index));
                Theme::instance().setMode(index);
                emit preferencesChanged();
            });
    form->addRow(QStringLiteral("主题"), m_theme);

    auto* rampRow = new QHBoxLayout();
    m_ramp = new QComboBox(appearance);
    for (int kind = 0; kind <= 2; ++kind)
    {
        m_ramp->addItem(HeatRamp::name(kind));
    }
    m_ramp->setCurrentIndex(m_settings.heatRamp());
    auto* preview = new RampPreview(m_settings.heatRamp(), appearance);
    connect(m_ramp, &QComboBox::currentIndexChanged, this,
            [this, preview](int index)
            {
                m_settings.setHeatRamp(index);
                preview->setKind(index);
                emit preferencesChanged();
            });
    rampRow->addWidget(m_ramp, 1);
    rampRow->addWidget(preview, 2);
    form->addRow(QStringLiteral("热力色带"), rampRow);

    m_layout = new QComboBox(appearance);
    const LayoutRepository& repo = LayoutRepository::instance();
    for (const QString& id : repo.availableIds())
    {
        m_layout->addItem(repo.displayName(id), id);
    }
    const int layoutIndex = m_layout->findData(m_settings.layoutId());
    m_layout->setCurrentIndex(qMax(0, layoutIndex));
    connect(m_layout, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                m_settings.setLayoutId(m_layout->itemData(index).toString());
                emit preferencesChanged();
            });
    form->addRow(QStringLiteral("键盘布局"), m_layout);

    m_weekStart = new QComboBox(appearance);
    m_weekStart->addItem(QStringLiteral("周一"), 1);
    m_weekStart->addItem(QStringLiteral("周日"), 7);
    m_weekStart->setCurrentIndex(m_settings.weekStart() == 7 ? 1 : 0);
    connect(m_weekStart, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                m_settings.setWeekStart(m_weekStart->itemData(index).toInt());
                emit preferencesChanged();
            });
    form->addRow(QStringLiteral("每周起始"), m_weekStart);

    m_reduceMotion = new QCheckBox(QStringLiteral("减少动画"), appearance);
    m_reduceMotion->setChecked(m_settings.reduceMotion());
    connect(m_reduceMotion, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                m_settings.setReduceMotion(checked);
                emit preferencesChanged();
            });
    form->addRow(QString(), m_reduceMotion);

    m_hourlyChart = new QCheckBox(QStringLiteral("显示时段分布图"), appearance);
    m_hourlyChart->setChecked(m_settings.hourlyChart());
    connect(m_hourlyChart, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                m_settings.setHourlyChart(checked);
                emit preferencesChanged();
            });
    form->addRow(QString(), m_hourlyChart);

    layout->addWidget(appearance);
    layout->addStretch(1);
    return page;
}

QWidget* SettingsDialog::buildHotkeyTab()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* group = new QGroupBox(QStringLiteral("全局快捷键"), page);
    auto* groupLayout = new QVBoxLayout(group);

    m_hotkeyEnabled = new QCheckBox(QStringLiteral("启用全局快捷键（随时唤起统计面板）"), group);
    m_hotkeyEnabled->setChecked(m_settings.hotkeyEnabled());
    groupLayout->addWidget(m_hotkeyEnabled);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(QStringLiteral("组合键"), group));
    m_hotkeyEdit = new QKeySequenceEdit(m_settings.hotkey(), group);
    m_hotkeyEdit->setMaximumSequenceLength(1);
    row->addWidget(m_hotkeyEdit, 1);
    groupLayout->addLayout(row);

    m_hotkeyStatus = hintLabel(QString(), group);
    groupLayout->addWidget(m_hotkeyStatus);

    groupLayout->addWidget(hintLabel(QStringLiteral("说明：全局快捷键会从系统中被本程序截获（Windows 的固有行为），"
                                                  "因此默认关闭；组合键必须包含 Ctrl / Alt / Shift / Win 中的至少一个修饰键。"),
                                    group));

    const auto notify = [this]
    {
        m_settings.setHotkey(m_hotkeyEdit->keySequence());
        m_settings.setHotkeyEnabled(m_hotkeyEnabled->isChecked());
        emit hotkeyChanged(m_hotkeyEnabled->isChecked(), m_hotkeyEdit->keySequence());
    };
    connect(m_hotkeyEnabled, &QCheckBox::toggled, this, [notify](bool) { notify(); });
    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged, this, [notify](const QKeySequence&) { notify(); });

    layout->addWidget(group);
    layout->addStretch(1);
    return page;
}

QWidget* SettingsDialog::buildDataTab()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* statsGroup = new QGroupBox(QStringLiteral("统计口径"), page);
    auto* form = new QFormLayout(statsGroup);
    m_errorMode = new QComboBox(statsGroup);
    m_errorMode->addItem(QStringLiteral("归因：哪个键最常被打错（推荐）"));
    m_errorMode->addItem(QStringLiteral("字面：删除键按了多少次"));
    m_errorMode->setCurrentIndex(static_cast<int>(m_settings.errorMode()));
    connect(m_errorMode, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                m_settings.setErrorMode(static_cast<Settings::ErrorMode>(index));
                emit preferencesChanged();
            });
    form->addRow(QStringLiteral("错误统计"), m_errorMode);
    form->addRow(hintLabel(QStringLiteral("「归因」把每次退格/删除记到刚打错的那个键上；"
                                          "「字面」只统计删除键本身的按下次数。两者都基于同一份数据，可随时切换。"),
                           statsGroup));
    layout->addWidget(statsGroup);

    auto* dataGroup = new QGroupBox(QStringLiteral("数据"), page);
    auto* dataLayout = new QVBoxLayout(dataGroup);
    m_dataPath = new QLabel(StatsStore::dataDir(), dataGroup);
    m_dataPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_dataPath->setFont(Theme::instance().font(12));
    dataLayout->addWidget(m_dataPath);

    auto* buttons = new QHBoxLayout();
    auto* openButton = new QPushButton(QStringLiteral("打开数据目录"), dataGroup);
    connect(openButton, &QPushButton::clicked, this,
            []
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(StatsStore::dataDir()));
            });
    buttons->addWidget(openButton);

    auto* clearButton = new QPushButton(QStringLiteral("清空所有统计数据"), dataGroup);
    connect(clearButton, &QPushButton::clicked, this,
            [this]
            {
                const auto answer = QMessageBox::question(
                    this, QStringLiteral("确认清空"),
                    QStringLiteral("将删除全部按键统计（不可恢复，建议先复制数据目录备份）。确定继续吗？"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer == QMessageBox::Yes)
                {
                    emit clearDataRequested();
                }
            });
    buttons->addWidget(clearButton);
    buttons->addStretch(1);
    dataLayout->addLayout(buttons);
    dataLayout->addWidget(hintLabel(QStringLiteral("只保存逐键的聚合计数（物理键 + 次数），不保存按键序列、"
                                                   "不记录窗口标题、不联网。"),
                                    dataGroup));
    layout->addWidget(dataGroup);
    layout->addStretch(1);
    return page;
}

QWidget* SettingsDialog::buildAboutTab()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("keyCounter %1").arg(QApplication::applicationVersion()), page);
    title->setFont(Theme::instance().font(15, QFont::DemiBold));
    layout->addWidget(title);

    auto* detail = new QLabel(QStringLiteral("基于 Qt %1 · 采集：Windows Raw Input（不注入、不挂钩）· "
                                             "存储：本地 SQLite")
                                  .arg(QString::fromLatin1(qVersion())),
                              page);
    detail->setWordWrap(true);
    layout->addWidget(detail);

    layout->addWidget(hintLabel(QStringLiteral("<b>已知限制</b><br/>"
                                               "· 以管理员身份运行的窗口（提权进程）中的按键无法被普通权限进程接收（Windows UIPI 限制）；<br/>"
                                               "· UAC 弹窗、锁屏界面属于安全桌面，同样不会投递按键；<br/>"
                                               "· Fn 键由键盘硬件处理，不产生系统键码，无法统计。"),
                                    page));

    layout->addWidget(hintLabel(QStringLiteral("<b>统计说明</b><br/>"
                                               "· 长按不算多次，只在按下瞬间计 1 次；<br/>"
                                               "· 左右 Shift / Ctrl、主回车与小键盘回车等按物理键位分别统计；<br/>"
                                               "· 数据每 15 秒或跨零点、休眠、退出时落盘。"),
                                    page));
    layout->addStretch(1);
    return page;
}

void SettingsDialog::refreshAutostartState()
{
    if (m_autostartHint == nullptr)
    {
        return;
    }
    const bool enabled = m_settings.autostart();
    if (m_autostart->isChecked() != enabled)
    {
        const QSignalBlocker blocker(m_autostart);
        m_autostart->setChecked(enabled);
    }
    m_autostartHint->setText(enabled
                                 ? QStringLiteral("已登记：HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run")
                                 : QStringLiteral("未登记开机自启"));
}

void SettingsDialog::setHotkeyStatus(bool ok, const QString& message)
{
    if (m_hotkeyStatus == nullptr)
    {
        return;
    }
    const Palette& palette = Theme::instance().palette();
    m_hotkeyStatus->setText(message.isEmpty() ? QStringLiteral("当前组合键：%1")
                                                    .arg(m_settings.hotkey().toString(QKeySequence::NativeText))
                                              : message);
    m_hotkeyStatus->setStyleSheet(QStringLiteral("color:%1;")
                                      .arg((ok ? palette.textFaint : palette.danger).name()));
}

}  // namespace kc
