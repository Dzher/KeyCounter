// keyCounter - 键盘布局解析与校验
#include "LayoutModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

namespace kc
{
namespace
{

const QStringList kBuiltinIds = {QStringLiteral("ansi-104"), QStringLiteral("tkl-87"),
                                 QStringLiteral("compact-68")};

bool parsePid(const QJsonValue& value, quint16* pid, bool* countable)
{
    *countable = true;
    if (value.isBool() && !value.toBool())
    {
        *pid = kInvalidPid;
        *countable = false;
        return true;
    }
    if (value.isDouble())
    {
        *pid = static_cast<quint16>(value.toInt());
        return true;
    }
    if (value.isString())
    {
        bool ok = false;
        const quint16 v = static_cast<quint16>(value.toString().toUShort(&ok, 0));
        if (!ok)
        {
            return false;
        }
        *pid = v;
        return true;
    }
    return false;
}

}  // namespace

const KeyGeometry* KeyboardLayout::find(quint16 pid) const
{
    const auto it = indexByPid.constFind(pid);
    if (it == indexByPid.constEnd())
    {
        return nullptr;
    }
    return &keys.at(it.value());
}

QString KeyboardLayout::validate() const
{
    if (keys.isEmpty())
    {
        return QStringLiteral("布局为空");
    }

    QSet<quint16> seen;
    for (const KeyGeometry& k : keys)
    {
        if (!k.countable || k.pid == kInvalidPid)
        {
            continue;
        }
        if (seen.contains(k.pid))
        {
            return QStringLiteral("pid 0x%1 重复出现").arg(k.pid, 3, 16, QLatin1Char('0'));
        }
        seen.insert(k.pid);

        if (k.cell.width() <= 0.0 || k.cell.height() <= 0.0)
        {
            return QStringLiteral("pid 0x%1 尺寸非法").arg(k.pid, 3, 16, QLatin1Char('0'));
        }
    }

    // 重叠检测（含 E0/E1 变体不算重复：Left Shift/RShift 是不同 pid，但可能被同一布局重复放置）
    for (int i = 0; i < keys.size(); ++i)
    {
        for (int j = i + 1; j < keys.size(); ++j)
        {
            if (keys[i].cell.intersects(keys[j].cell))
            {
                return QStringLiteral("键位重叠：\"%1\" 与 \"%2\"").arg(keys[i].label, keys[j].label);
            }
        }
    }
    return QString();
}

std::shared_ptr<const KeyboardLayout> KeyboardLayout::parse(const QByteArray& json, QString* error)
{
    const auto fail = [error](const QString& msg) -> std::shared_ptr<const KeyboardLayout>
    {
        if (error != nullptr)
        {
            *error = msg;
        }
        return nullptr;
    };

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        return fail(QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString()));
    }
    const QJsonObject root = doc.object();

    auto layout = std::make_shared<KeyboardLayout>();
    layout->id = root.value(QStringLiteral("id")).toString();
    layout->name = root.value(QStringLiteral("name")).toString(layout->id);
    layout->gapU = root.value(QStringLiteral("gap")).toDouble(0.08);
    if (layout->id.isEmpty())
    {
        return fail(QStringLiteral("缺少 id"));
    }

    const QJsonArray clusters = root.value(QStringLiteral("clusters")).toArray();
    for (const QJsonValue& clusterValue : clusters)
    {
        const QJsonObject cluster = clusterValue.toObject();
        const int clusterIndex = layout->clusterTitles.size();
        layout->clusterTitles.append(cluster.value(QStringLiteral("title")).toString());

        const QJsonArray origin = cluster.value(QStringLiteral("origin")).toArray();
        const double originX = origin.size() > 0 ? origin.at(0).toDouble() : 0.0;
        const double originY = origin.size() > 1 ? origin.at(1).toDouble() : 0.0;

        const QJsonArray rows = cluster.value(QStringLiteral("rows")).toArray();
        for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            const QJsonArray items = rows.at(rowIndex).toArray();
            double cursor = originX;
            for (const QJsonValue& itemValue : items)
            {
                const QJsonObject item = itemValue.toObject();
                if (item.contains(QStringLiteral("gap")))
                {
                    cursor += item.value(QStringLiteral("gap")).toDouble();
                    continue;
                }

                KeyGeometry key;
                bool countable = true;
                if (!parsePid(item.value(QStringLiteral("pid")), &key.pid, &countable))
                {
                    return fail(QStringLiteral("非法 pid：%1").arg(item.value(QStringLiteral("pid")).toVariant().toString()));
                }
                key.countable = countable && item.value(QStringLiteral("countable")).toBool(true);

                const double w = item.value(QStringLiteral("w")).toDouble(1.0);
                const double h = item.value(QStringLiteral("h")).toDouble(1.0);
                if (item.contains(QStringLiteral("x")))
                {
                    cursor = originX + item.value(QStringLiteral("x")).toDouble();
                }
                const double y = originY + rowIndex;

                key.label = item.value(QStringLiteral("label")).toString();
                key.sub = item.value(QStringLiteral("sub")).toString();
                key.cluster = clusterIndex;
                key.cell = QRectF(cursor, y, w, h);
                layout->keys.append(key);

                if (key.countable && key.pid != kInvalidPid && !layout->indexByPid.contains(key.pid))
                {
                    layout->indexByPid.insert(key.pid, layout->keys.size() - 1);
                }
                cursor += w;
            }
        }
    }

    for (const KeyGeometry& k : layout->keys)
    {
        layout->widthU = qMax(layout->widthU, k.cell.right());
        layout->heightU = qMax(layout->heightU, k.cell.bottom());
    }

    const QString problem = layout->validate();
    if (!problem.isEmpty())
    {
        return fail(QStringLiteral("%1：%2").arg(layout->id, problem));
    }
    return layout;
}

LayoutRepository& LayoutRepository::instance()
{
    static LayoutRepository repo;
    return repo;
}

QStringList LayoutRepository::availableIds() const
{
    // 内置布局来自 qrc；用户自定义布局（%APPDATA%/keyCounter/layouts/*.json）为后续扩展点
    return kBuiltinIds;
}

QString LayoutRepository::displayName(const QString& id) const
{
    if (const auto layout = const_cast<LayoutRepository*>(this)->layout(id))
    {
        return layout->name;
    }
    return id;
}

std::shared_ptr<const KeyboardLayout> LayoutRepository::layout(const QString& id)
{
    const QString key = id.isEmpty() ? defaultLayoutId() : id;
    if (const auto it = m_cache.constFind(key); it != m_cache.constEnd())
    {
        return it.value();
    }

    QFile file(QStringLiteral(":/layouts/%1.json").arg(key));
    std::shared_ptr<const KeyboardLayout> loaded;
    if (file.open(QIODevice::ReadOnly))
    {
        loaded = KeyboardLayout::parse(file.readAll(), nullptr);
    }
    if (!loaded)
    {
        if (key != defaultLayoutId())
        {
            return layout(defaultLayoutId());
        }
        return nullptr;
    }
    m_cache.insert(key, loaded);
    return loaded;
}

}  // namespace kc
