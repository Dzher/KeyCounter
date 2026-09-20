// keyCounter - 键盘布局模型（声明式 JSON → 等比例几何）
#pragma once

#include <QByteArray>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

#include "core/KeyTypes.h"

namespace kc
{

/// 单个键位，坐标单位为 U（1U = 标准键宽），不含视觉间隙
struct KeyGeometry
{
    quint16 pid = 0;
    QString label;
    QString sub;
    QRectF cell;
    int cluster = 0;
    bool countable = true;
};

/// 一套实体键盘布局
class KeyboardLayout
{
public:
    /// 从 JSON 描述构建布局（内置布局数据也走这条路径，便于日后支持用户自定义布局）
    static std::shared_ptr<const KeyboardLayout> parse(const QByteArray& json, QString* error);

    QString id;
    QString name;
    double gapU = 0.08;
    double widthU = 0;
    double heightU = 0;
    QVector<KeyGeometry> keys;
    QStringList clusterTitles;
    QHash<quint16, int> indexByPid;  ///< pid → keys 下标

    const KeyGeometry* find(quint16 pid) const;
    /// 校验：返回空串表示合法，否则返回问题描述（单测使用）
    QString validate() const;
};

/// 布局仓库：内置布局来自 qrc，用户布局来自 %APPDATA%/keyCounter/layouts/*.json
class LayoutRepository
{
public:
    static LayoutRepository& instance();

    QStringList availableIds() const;
    QString displayName(const QString& id) const;
    /// 带缓存加载；失败返回 nullptr
    std::shared_ptr<const KeyboardLayout> layout(const QString& id);
    static QString defaultLayoutId() { return QStringLiteral("ansi-104"); }

private:
    LayoutRepository() = default;
    QHash<QString, std::shared_ptr<const KeyboardLayout>> m_cache;
};

}  // namespace kc
