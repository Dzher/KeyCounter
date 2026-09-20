// keyCounter - 单实例实现
#include "SingleInstance.h"

#include <QLocalSocket>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace kc
{

SingleInstance::SingleInstance(QString key, QObject* parent) : QObject(parent), m_key(std::move(key))
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this,
            [this]
            {
                while (QLocalSocket* socket = m_server.nextPendingConnection())
                {
                    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
                    connect(socket, &QLocalSocket::readyRead, socket,
                            [this, socket]
                            {
                                const QByteArray payload = socket->readAll();
                                if (payload.startsWith("show"))
                                {
                                    emit activationRequested();
                                }
                                socket->disconnectFromServer();
                            });
                }
            });
}

SingleInstance::~SingleInstance()
{
#ifdef Q_OS_WIN
    if (m_mutex != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(m_mutex));
        m_mutex = nullptr;
    }
#endif
}

bool SingleInstance::tryLock()
{
#ifdef Q_OS_WIN
    // 命名互斥体：进程退出（含崩溃）后由内核自动释放，不会留下"僵尸锁"
    const std::wstring name = (QStringLiteral("Local\\") + m_key).toStdWString();
    m_mutex = CreateMutexW(nullptr, FALSE, name.c_str());
    const DWORD lastError = GetLastError();
    if (m_mutex == nullptr)
    {
        return true;  // 互斥体创建失败（极少见）：不阻塞启动
    }
    if (lastError == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(static_cast<HANDLE>(m_mutex));
        m_mutex = nullptr;
        return false;
    }
#else
    Q_UNUSED(m_key);
#endif

    // 唤起通道（命名管道）：清理上一次异常退出可能留下的残留，再监听
    if (!m_server.listen(m_key))
    {
        QLocalServer::removeServer(m_key);
        m_server.listen(m_key);  // 仍失败也不阻塞启动，只是收不到唤起请求
    }
    return true;
}

bool SingleInstance::notifyExisting()
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(1000))
    {
        return false;
    }
    socket.write("show");
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

}  // namespace kc
