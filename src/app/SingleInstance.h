// keyCounter - 单实例：命名互斥体判定 + QLocalServer 唤起已有窗口
#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

namespace kc
{

class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(QString key, QObject* parent = nullptr);
    ~SingleInstance() override;

    /// 成为主实例返回 true；已有实例在运行时返回 false
    bool tryLock();

    /// 通知已有实例显示面板
    bool notifyExisting();

signals:
    void activationRequested();

private:
    QString m_key;
    QLocalServer m_server;
    void* m_mutex = nullptr;  ///< HANDLE（Windows 命名互斥体）
};

}  // namespace kc
