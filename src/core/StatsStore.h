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

    /// 默认数据目录（%APPDATA%/keyCounter，或便携模式 exe/data）
    static QString defaultDataDir();
    /// 数据文件名
    static QString databaseFileName();
    /// 指定数据目录下的数据库完整路径
    static QString databasePathIn(const QString& dir);
    /// 默认数据目录下的数据库完整路径
    static QString defaultDatabasePath();

    /// 迁移数据目录内容（数据库 + WAL/SHM + backup/），成功返回 true，失败时 error 说明原因且不改变任何文件
    static bool migrateDataDirectory(const QString& fromDir, const QString& toDir, bool overwrite, QString* error);

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
