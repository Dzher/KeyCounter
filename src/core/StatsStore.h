// keyCounter - SQLite 存储（运行在独立线程）
#pragma once

#include <QObject>
#include <QString>

#include "KeyTypes.h"

class QSqlDatabase;

namespace kc
{

/// 唯一的数据库写入口。所有公开槽都在存储线程执行。
class StatsStore : public QObject
{
    Q_OBJECT
public:
    explicit StatsStore(QObject* parent = nullptr);
    ~StatsStore() override;

    /// DB 文件名
    static QString defaultDatabasePath();
    /// 解析后的数据目录（%APPDATA%/keyCounter 或便携模式 exe/data）
    static QString dataDir();

public slots:
    void open(const QString& dbPath);
    void applyBatch(const kc::DayBatch& batch);
    void flush();
    void clearAll();
    void shutdown();

signals:
    void opened(bool ok, const QString& error, int firstDataDay);
    /// 每次成功落盘后递增，UI 据此失效缓存
    void batchingApplied(quint64 revision, int touchedDay);
    /// 数据文件损坏已被隔离（原文件保留在旁）
    void integrityWarning(const QString& message);
    void failed(const QString& message);

private:
    bool ensureSchema(QString* error);
    bool writeMeta(const QString& key, const QString& value);
    quint64 readMetaInt(const QString& key, quint64 fallback);

    QString m_path;
    QSqlDatabase* m_db = nullptr;
    quint64 m_revision = 0;
    bool m_ready = false;
};

}  // namespace kc
