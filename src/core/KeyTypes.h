// keyCounter - 公共类型与常量
// 物理键标识：pid = makeCode | (E0 ? 0x100 : 0) | (E1 ? 0x200 : 0)
#pragma once

#include <array>
#include <cstdint>

#include <QHash>
#include <QPair>
#include <QVector>
#include <QtGlobal>

namespace kc
{

/// pid 取值范围（10 bit：makeCode + E0/E1 两位）
inline constexpr quint16 kMaxPid = 1024;
inline constexpr quint16 kInvalidPid = 0;

constexpr quint16 encodePid(quint8 makeCode, bool e0, bool e1) noexcept
{
    return static_cast<quint16>(makeCode) | (e0 ? quint16(0x100) : quint16(0)) | (e1 ? quint16(0x200) : quint16(0));
}

constexpr bool pidIsExtended(quint16 pid) noexcept { return (pid & 0x100) != 0; }
constexpr bool pidIsE1(quint16 pid) noexcept { return (pid & 0x200) != 0; }
constexpr quint8 pidMakeCode(quint16 pid) noexcept { return static_cast<quint8>(pid & 0xFF); }

/// 一次落盘的最小单元：某一天的逐键计数（仅当前这一天常驻内存）
struct DayBatch
{
    qint32 day = 0;  ///< QDate::toJulianDay()
    std::array<quint32, kMaxPid> press{};
    std::array<quint32, kMaxPid> corr{};
    std::array<quint32, 24> hours{};

    bool isEmpty() const noexcept
    {
        for (quint32 v : press)
        {
            if (v != 0)
            {
                return false;
            }
        }
        for (quint32 v : corr)
        {
            if (v != 0)
            {
                return false;
            }
        }
        for (quint32 v : hours)
        {
            if (v != 0)
            {
                return false;
            }
        }
        return true;
    }

    void clear() noexcept
    {
        press.fill(0);
        corr.fill(0);
        hours.fill(0);
    }
};

/// 单个键在某区间内的聚合值
struct KeyStat
{
    quint32 press = 0;  ///< 按下次数（不含自动重复）
    quint32 corr = 0;   ///< 归因错误次数（被退格/删除修正）
};

/// 某区间的完整统计结果（UI 与托盘共用）
struct PeriodStats
{
    QHash<quint16, KeyStat> keys;
    QVector<QPair<int, KeyStat>> days;  ///< 仅含有数据的日期，按天升序
    std::array<quint32, 24> hours{};
    quint64 totalPress = 0;
    quint64 totalCorr = 0;
    quint32 maxKeyPress = 0;
    quint16 maxKeyPid = kInvalidPid;
    int activeDays = 0;
    bool valid = false;  ///< false 表示查询失败
};

}  // namespace kc
