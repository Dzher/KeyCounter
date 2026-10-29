// keyCounter - SQLite 存储实现
#include "StatsStore.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QVariant>
#include <QVector>

namespace kc
{
namespace
{

constexpr int kSchemaVersion = 1;
constexpr int kKeepBackups = 7;
const QString kConnectionName = QStringLiteral("keycounter-store");

QString timestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
}

/// Windows 路径比较：大小写与分隔符都不敏感
bool sameDirectory(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty())
    {
        return a.isEmpty() && b.isEmpty();
    }
    return QDir::cleanPath(QDir(a).absolutePath())
               .compare(QDir::cleanPath(QDir(b).absolutePath()), Qt::CaseInsensitive) == 0;
}

bool copyDirectory(const QString& from, const QString& to, QString* error)
{
    if (!QDir().mkpath(to))
    {
        *error = QStringLiteral("无法创建目录：%1").arg(to);
        return false;
    }
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext())
    {
        const QString source = it.next();
        const QString target = to + source.mid(from.size());
        if (it.fileInfo().isDir())
        {
            if (!copyDirectory(source, target, error))
            {
                return false;
            }
        }
        else if (!QFile::copy(source, target))
        {
            *error = QStringLiteral("无法复制文件：%1").arg(source);
            return false;
        }
    }
    return true;
}

/// 移动文件/目录：同卷直接改名，跨卷（如换盘）退化为复制 + 删除
bool movePath(const QString& from, const QString& to, QString* error)
{
    if (QFile::rename(from, to))
    {
        return true;
    }

    const QFileInfo info(from);
    if (info.isDir())
    {
        if (!copyDirectory(from, to, error))
        {
            return false;
        }
        QDir(from).removeRecursively();
        return true;
    }
    if (!QFile::copy(from, to))
    {
        *error = QStringLiteral("无法移动文件：%1 → %2").arg(from, to);
        return false;
    }
    if (!QFile::remove(from))
    {
        *error = QStringLiteral("文件已复制但无法删除源文件：%1").arg(from);
        return false;
    }
    return true;
}

}  // namespace

QString StatsStore::defaultDataDir()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    if (QFileInfo::exists(exeDir + QStringLiteral("/portable.flag")))
    {
        return exeDir + QStringLiteral("/data");
    }
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty())
    {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
    if (base.isEmpty())
    {
        base = QDir::homePath();
    }
    return base + QStringLiteral("/keyCounter");
}

QString StatsStore::databaseFileName()
{
    return QStringLiteral("keycounter.db");
}

QString StatsStore::databasePathIn(const QString& dir)
{
    return QDir::cleanPath(dir + QLatin1Char('/') + databaseFileName());
}

QString StatsStore::defaultDatabasePath()
{
    return databasePathIn(defaultDataDir());
}

bool StatsStore::migrateDataDirectory(const QString& fromDir, const QString& toDir, bool overwrite, QString* error)
{
    QString ignored;
    QString* err = (error != nullptr) ? error : &ignored;
    *err = QString();

    const QString source = QDir::cleanPath(fromDir);
    const QString target = QDir::cleanPath(toDir);
    if (source.isEmpty() || target.isEmpty())
    {
        *err = QStringLiteral("数据目录路径为空。");
        return false;
    }
    if (sameDirectory(source, target))
    {
        return true;  // 原地不动
    }
    if (target.startsWith(source + QLatin1Char('/'), Qt::CaseInsensitive) ||
        source.startsWith(target + QLatin1Char('/'), Qt::CaseInsensitive))
    {
        *err = QStringLiteral("新目录不能是当前数据目录的子目录（或反之）。");
        return false;
    }
    if (!QDir().mkpath(target))
    {
        *err = QStringLiteral("无法创建目录：%1").arg(target);
        return false;
    }

    // 可写性 / 磁盘可用的提前探测，避免搬到一半才发现盘不可写
    QTemporaryFile probe(target + QStringLiteral("/.kc-write-probe-XXXXXX"));
    if (!probe.open())
    {
        *err = QStringLiteral("目标目录不可写：%1").arg(target);
        return false;
    }
    probe.close();

    const QString dbName = databaseFileName();
    const QString targetDb = target + QLatin1Char('/') + dbName;
    if (QFile::exists(targetDb) && !overwrite)
    {
        *err = QStringLiteral("目标目录已有数据文件：%1").arg(targetDb);
        return false;
    }

    // 逐项搬移；任一步失败就回滚已移动的项，保证不会出现「一半在新目录、一半在旧目录」
    QVector<QPair<QString, QString>> moved;
    const auto rollback = [&moved]()
    {
        for (auto it = moved.crbegin(); it != moved.crend(); ++it)
        {
            QString ignoredError;
            movePath(it->second, it->first, &ignoredError);
        }
        moved.clear();
    };

    const QStringList items = {dbName, dbName + QStringLiteral("-wal"), dbName + QStringLiteral("-shm"),
                               QStringLiteral("backup")};
    for (const QString& item : items)
    {
        const QString from = source + QLatin1Char('/') + item;
        const QString to = target + QLatin1Char('/') + item;
        if (!QFileInfo::exists(from))
        {
            continue;
        }
        // 空的 WAL/SHM 没有搬运价值；备份目录只在目标没有时整目录搬走
        if (item == QStringLiteral("backup"))
        {
            if (QFileInfo::exists(to) || QDir(from).isEmpty())
            {
                continue;
            }
        }
        else
        {
            if (QFileInfo::exists(to) && !QFile::remove(to))
            {
                *err = QStringLiteral("无法覆盖目标文件：%1").arg(to);
                rollback();
                return false;
            }
        }
        if (!movePath(from, to, err))
        {
            rollback();
            return false;
        }
        moved.append({from, to});
    }

    return true;
}

StatsStore::StatsStore(QObject* parent) : QObject(parent) {}

StatsStore::~StatsStore()
{
    if (m_db != nullptr)
    {
        shutdown();
    }
}

bool StatsStore::ensureSchema(QString* error)
{
    const auto exec = [this, error](const QString& sql) -> bool
    {
        QSqlQuery q(*m_db);
        if (!q.exec(sql))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("%1 (%2)").arg(q.lastError().text(), sql);
            }
            return false;
        }
        return true;
    };

    static const QStringList kStatements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS key_daily("
                       "  day INTEGER NOT NULL,"
                       "  pid INTEGER NOT NULL,"
                       "  press INTEGER NOT NULL DEFAULT 0,"
                       "  corr INTEGER NOT NULL DEFAULT 0,"
                       "  PRIMARY KEY(day,pid)) WITHOUT ROWID"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS day_hour("
                       "  day INTEGER NOT NULL,"
                       "  hour INTEGER NOT NULL,"
                       "  press INTEGER NOT NULL DEFAULT 0,"
                       "  corr INTEGER NOT NULL DEFAULT 0,"
                       "  PRIMARY KEY(day,hour)) WITHOUT ROWID"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS key_meta("
                       "  pid INTEGER PRIMARY KEY,"
                       "  vk INTEGER,"
                       "  first_day INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS meta(k TEXT PRIMARY KEY, v TEXT)"),
    };

    for (const QString& sql : kStatements)
    {
        if (!exec(sql))
        {
            return false;
        }
    }

    QSqlQuery version(*m_db);
    version.prepare(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion));
    return version.exec();
}

bool StatsStore::writeMeta(const QString& key, const QString& value)
{
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("INSERT INTO meta(k,v) VALUES(?1,?2) "
                             "ON CONFLICT(k) DO UPDATE SET v=excluded.v"));
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

quint64 StatsStore::readMetaInt(const QString& key, quint64 fallback)
{
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("SELECT v FROM meta WHERE k=?1"));
    q.addBindValue(key);
    if (!q.exec() || !q.next())
    {
        return fallback;
    }
    bool ok = false;
    const quint64 v = q.value(0).toString().toULongLong(&ok);
    return ok ? v : fallback;
}

void StatsStore::open(const QString& dbPath)
{
    m_path = dbPath;
    const QFileInfo info(m_path);
    if (!QDir().mkpath(info.absolutePath()))
    {
        emit opened(false, QStringLiteral("无法创建数据目录：%1").arg(info.absolutePath()), 0);
        return;
    }

    const auto connectAndPrepare = [this]() -> bool
    {
        m_db = new QSqlDatabase(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName));
        m_db->setDatabaseName(m_path);
        if (!m_db->open())
        {
            return false;
        }
        QSqlQuery pragma(*m_db);
        pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
        pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));
        pragma.exec(QStringLiteral("PRAGMA temp_store=MEMORY"));
        pragma.exec(QStringLiteral("PRAGMA cache_size=-2000"));
        return true;
    };

    if (!connectAndPrepare())
    {
        const QString err = m_db != nullptr ? m_db->lastError().text() : QStringLiteral("unknown");
        emit opened(false, QStringLiteral("打开数据库失败：%1").arg(err), 0);
        return;
    }

    // 完整性自检：失败则隔离旧文件后重建（绝不静默丢数据）
    bool healthy = false;
    {
        QSqlQuery check(*m_db);
        if (check.exec(QStringLiteral("PRAGMA quick_check")) && check.next())
        {
            healthy = check.value(0).toString().compare(QStringLiteral("ok"), Qt::CaseInsensitive) == 0;
        }
    }
    if (!healthy)
    {
        m_db->close();
        delete m_db;
        m_db = nullptr;
        QSqlDatabase::removeDatabase(kConnectionName);

        const QString quarantine = info.absolutePath() + QStringLiteral("/keycounter.corrupt-") + timestamp() +
                                   QStringLiteral(".db");
        QFile::rename(m_path, quarantine);
        QFile::remove(m_path + QStringLiteral("-wal"));
        QFile::remove(m_path + QStringLiteral("-shm"));

        if (!connectAndPrepare())
        {
            emit opened(false, QStringLiteral("数据库损坏且重建失败"), 0);
            return;
        }
        emit integrityWarning(QStringLiteral("原数据文件校验失败，已隔离保留为 %1 并新建空库。").arg(quarantine));
    }

    QString error;
    if (!ensureSchema(&error))
    {
        emit opened(false, error, 0);
        return;
    }

    // 首次使用标记
    const int today = static_cast<int>(QDate::currentDate().toJulianDay());
    if (readMetaInt(QStringLiteral("install_day"), 0) == 0)
    {
        writeMeta(QStringLiteral("install_day"), QString::number(today));
    }

    int firstDataDay = 0;
    {
        QSqlQuery q(*m_db);
        if (q.exec(QStringLiteral("SELECT MIN(day) FROM key_daily")) && q.next() && !q.value(0).isNull())
        {
            firstDataDay = q.value(0).toInt();
        }
        if (firstDataDay == 0 && q.exec(QStringLiteral("SELECT v FROM meta WHERE k='first_data_day'")) && q.next())
        {
            firstDataDay = q.value(0).toInt();
        }
    }

    m_ready = true;
    emit opened(true, QString(), firstDataDay);
}

void StatsStore::applyBatch(const kc::DayBatch& batch)
{
    if (!m_ready || m_db == nullptr || batch.isEmpty())
    {
        return;
    }

    const bool firstOfSession = (m_revision == 0);

    if (!m_db->transaction())
    {
        emit failed(QStringLiteral("开启事务失败：%1").arg(m_db->lastError().text()));
        return;
    }

    QSqlQuery keyQuery(*m_db);
    keyQuery.prepare(QStringLiteral("INSERT INTO key_daily(day,pid,press,corr) VALUES(?1,?2,?3,?4) "
                                    "ON CONFLICT(day,pid) DO UPDATE SET "
                                    "press=press+excluded.press, corr=corr+excluded.corr"));

    QSqlQuery metaQuery(*m_db);
    metaQuery.prepare(QStringLiteral("INSERT INTO key_meta(pid,vk,first_day) VALUES(?1,0,?2) "
                                     "ON CONFLICT(pid) DO NOTHING"));

    bool ok = true;
    for (quint16 pid = 0; pid < kMaxPid && ok; ++pid)
    {
        const quint32 press = batch.press[pid];
        const quint32 corr = batch.corr[pid];
        if (press == 0 && corr == 0)
        {
            continue;
        }
        keyQuery.addBindValue(batch.day);
        keyQuery.addBindValue(pid);
        keyQuery.addBindValue(press);
        keyQuery.addBindValue(corr);
        ok = keyQuery.exec();

        metaQuery.addBindValue(pid);
        metaQuery.addBindValue(batch.day);
        ok = ok && metaQuery.exec();
    }

    QSqlQuery hourQuery(*m_db);
    hourQuery.prepare(QStringLiteral("INSERT INTO day_hour(day,hour,press,corr) VALUES(?1,?2,?3,0) "
                                     "ON CONFLICT(day,hour) DO UPDATE SET press=press+excluded.press"));
    for (int hour = 0; hour < 24 && ok; ++hour)
    {
        if (batch.hours[hour] == 0)
        {
            continue;
        }
        hourQuery.addBindValue(batch.day);
        hourQuery.addBindValue(hour);
        hourQuery.addBindValue(batch.hours[hour]);
        ok = hourQuery.exec();
    }

    if (!ok)
    {
        m_db->rollback();
        emit failed(QStringLiteral("写入失败：%1").arg(keyQuery.lastError().text()));
        return;
    }

    if (!m_db->commit())
    {
        m_db->rollback();
        emit failed(QStringLiteral("提交失败：%1").arg(m_db->lastError().text()));
        return;
    }

    writeMeta(QStringLiteral("last_day"), QString::number(batch.day));
    if (readMetaInt(QStringLiteral("first_data_day"), 0) == 0)
    {
        writeMeta(QStringLiteral("first_data_day"), QString::number(batch.day));
    }

    ++m_revision;
    emit batchingApplied(m_revision, batch.day);

    if (firstOfSession)
    {
        // 每次会话首次落盘前做一次一致性快照（VACUUM INTO 由 SQLite 保证一致性）
        const QString backupDir = QFileInfo(m_path).absolutePath() + QStringLiteral("/backup");
        if (QDir().mkpath(backupDir))
        {
            const QString target =
                backupDir + QStringLiteral("/keycounter-") + QDate::currentDate().toString(QStringLiteral("yyyyMMdd")) +
                QStringLiteral(".db");
            if (!QFile::exists(target))
            {
                QSqlQuery vacuum(*m_db);
                vacuum.exec(QStringLiteral("VACUUM INTO '%1'").arg(QString(target).replace(QLatin1Char('\''), QLatin1String("''"))));

                QDir dir(backupDir);
                QStringList files = dir.entryList({QStringLiteral("keycounter-*.db")}, QDir::Files, QDir::Name);
                while (files.size() > kKeepBackups)
                {
                    dir.remove(files.takeFirst());
                }
            }
        }
    }
}

void StatsStore::flush()
{
    // 每个批次都是同步事务提交，无需额外动作；保留接口用于关停前的收口
}

void StatsStore::clearAll()
{
    if (!m_ready || m_db == nullptr)
    {
        return;
    }
    QSqlQuery q(*m_db);
    q.exec(QStringLiteral("DELETE FROM key_daily"));
    q.exec(QStringLiteral("DELETE FROM day_hour"));
    q.exec(QStringLiteral("DELETE FROM key_meta"));
    q.exec(QStringLiteral("DELETE FROM meta WHERE k IN ('first_data_day','last_day')"));
    q.exec(QStringLiteral("VACUUM"));
    ++m_revision;
    emit batchingApplied(m_revision, 0);
}

void StatsStore::shutdown()
{
    if (m_db != nullptr)
    {
        if (m_db->isOpen())
        {
            QSqlQuery q(*m_db);
            q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
            m_db->close();
        }
        delete m_db;
        m_db = nullptr;
    }
    if (QSqlDatabase::contains(kConnectionName))
    {
        QSqlDatabase::removeDatabase(kConnectionName);
    }
    m_ready = false;
}

}  // namespace kc
