// keyCounter - 高频键排行（Top N 横向条形）
#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

namespace kc
{

class TopKeysWidget : public QWidget
{
    Q_OBJECT
public:
    struct Entry
    {
        quint16 pid = 0;
        QString label;
        quint32 press = 0;
        quint32 corr = 0;
    };

    explicit TopKeysWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    /// 榜单切换标签（如 高频 / 打错）
    void setTabs(const QStringList& tabs, int current);
    void setEntries(const QVector<Entry>& entries, int rampKind, bool showCorrections);
    void setEmptyHint(const QString& hint);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void entryActivated(quint16 pid);
    void tabChangeRequested(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QRectF tabRect(int index) const;

    QVector<Entry> m_entries;
    QStringList m_tabs;
    int m_tab = 0;
    QString m_title;
    QString m_emptyHint = QStringLiteral("本期还没有数据");
    int m_ramp = 0;
    bool m_showCorrections = true;
    int m_hover = -1;
};

}  // namespace kc
