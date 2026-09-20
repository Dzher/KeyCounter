"""keyCounter - 演示数据生成器（开发/验收用）。

往统计库里写入一份"像真人打字"的历史数据，用于检查热力图渲染效果：
  python tools/gen-demo-data.py --days 45 [--db 路径] [--yes]

注意：会直接写入数据库，请在程序未运行时执行；先用 --db 指向副本更安全。
"""
import argparse
import os
import random
import sqlite3
import sys
from datetime import date, timedelta

JULIAN_UNIX_EPOCH = 2440588

# 物理键 pid（与 keyCounter 的 (E1<<9)|(E0<<8)|makeCode 编码一致）
LETTERS = {
    0x10: "Q", 0x11: "W", 0x12: "E", 0x13: "R", 0x14: "T", 0x15: "Y", 0x16: "U", 0x17: "I",
    0x18: "O", 0x19: "P", 0x1E: "A", 0x1F: "S", 0x20: "D", 0x21: "F", 0x22: "G", 0x23: "H",
    0x24: "J", 0x25: "K", 0x26: "L", 0x2C: "Z", 0x2D: "X", 0x2E: "C", 0x2F: "V", 0x30: "B",
    0x31: "N", 0x32: "M",
}
DIGITS = {0x02: "1", 0x03: "2", 0x04: "3", 0x05: "4", 0x06: "5",
          0x07: "6", 0x08: "7", 0x09: "8", 0x0A: "9", 0x0B: "0"}
SYMBOLS = {0x0C: "-", 0x0D: "=", 0x1A: "[", 0x1B: "]", 0x27: ";", 0x28: "'",
           0x29: "`", 0x2B: "\\", 0x33: ",", 0x34: ".", 0x35: "/"}
CONTROL = {0x39: "Space", 0x0E: "Backspace", 0x1C: "Enter", 0x2A: "LShift", 0x36: "RShift",
           0x01D: "LCtrl", 0x38: "LAlt", 0x00F: "Tab", 0x001: "Esc",
           0x148: "Up", 0x150: "Down", 0x14B: "Left", 0x14D: "Right",
           0x153: "Del", 0x147: "Home", 0x14F: "End"}
FKEYS = {0x3B: "F1", 0x3C: "F2", 0x3D: "F3", 0x3E: "F4", 0x3F: "F5", 0x40: "F6",
         0x41: "F7", 0x42: "F8", 0x43: "F9", 0x44: "F10", 0x57: "F11", 0x58: "F12"}


def build_weights():
    weights = {}
    # 空格与高频字母（英文/代码输入的典型分布）
    weights[0x39] = 12.0     # Space
    for pid, weight in ((0x12, 6.0), (0x14, 5.4), (0x1E, 5.6), (0x18, 5.2), (0x17, 4.8), (0x31, 4.9),
                        (0x1F, 4.6), (0x23, 4.4), (0x13, 4.5),  (0x26, 3.4), (0x20, 3.4), (0x2E, 3.0),
                        (0x15, 2.6), (0x16, 2.6), (0x24, 1.6), (0x25, 1.2), (0x19, 2.2), (0x11, 1.8),
                        (0x1C, 3.2), (0x2D, 1.6), (0x32, 2.4), (0x2F, 1.6), (0x30, 1.8), (0x21, 1.8),
                        (0x22, 1.6)):
        weights[pid] = weight
    for pid in LETTERS:
        weights.setdefault(pid, 1.0)
    weights[0x0E] = 6.5      # Backspace  (打错率来源)
    weights[0x2A] = 5.0      # LShift
    weights[0x36] = 0.6      # RShift
    weights[0x01D] = 1.6     # LCtrl
    weights[0x38] = 0.5      # LAlt
    weights[0x00F] = 0.5     # Tab
    weights[0x33] = 1.4
    weights[0x0C] = 0.9
    for pid in DIGITS:
        weights[pid] = 1.6
    for pid in SYMBOLS:
        weights[pid] = weights.get(pid, 1.0)
    weights[0x1A] = 0.7
    weights[0x1B] = 0.7
    weights[0x35] = 0.8
    for pid in FKEYS:
        weights[pid] = 0.05
    for pid in (0x148, 0x150, 0x14B, 0x14D):
        weights[pid] = 0.25
    return {pid: weight for pid, weight in weights.items() if weight > 0}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--days", type=int, default=45)
    parser.add_argument("--db", default=os.path.join(os.environ.get("APPDATA", ""), "keyCounter", "keycounter.db"))
    parser.add_argument("--yes", action="store_true", help="跳过确认")
    parser.add_argument("--seed", type=int, default=20250530)
    args = parser.parse_args()

    if not args.yes:
        print("将写入演示数据到：%s" % args.db)
        print("确认请加 --yes")
        return 1

    random.seed(args.seed)
    weights = build_weights()
    pids = list(weights.keys())
    keys = [weights[p] for p in pids]

    con = sqlite3.connect(args.db)
    cur = con.cursor()
    cur.execute("PRAGMA journal_mode=WAL")
    cur.execute("""CREATE TABLE IF NOT EXISTS key_daily(
                     day INTEGER NOT NULL, pid INTEGER NOT NULL, press INTEGER NOT NULL DEFAULT 0,
                     corr INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(day,pid)) WITHOUT ROWID""")
    cur.execute("""CREATE TABLE IF NOT EXISTS day_hour(
                     day INTEGER NOT NULL, hour INTEGER NOT NULL, press INTEGER NOT NULL DEFAULT 0,
                     corr INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(day,hour)) WITHOUT ROWID""")
    cur.execute("CREATE TABLE IF NOT EXISTS meta(k TEXT PRIMARY KEY, v TEXT)")
    cur.execute("CREATE TABLE IF NOT EXISTS key_meta(pid INTEGER PRIMARY KEY, vk INTEGER, first_day INTEGER NOT NULL)")

    today = date.today()
    first_day = today - timedelta(days=args.days - 1)
    total = 0

    for offset in range(args.days):
        day = first_day + timedelta(days=offset)
        weekday = day.weekday()
        # 工作日打字多，周末少；今天只统计到当前时刻
        base = 14000 if weekday < 5 else 4200
        if day == today:
            base = int(base * 0.55)
        day_total = int(random.gauss(base, base * 0.18))
        day_total = max(800, day_total)

        julian = (day - date(1970, 1, 1)).days + JULIAN_UNIX_EPOCH
        counts = {}
        for _ in range(day_total):
            pid = random.choices(pids, weights=keys, k=1)[0]
            counts[pid] = counts.get(pid, 0) + 1

        # 打错：约 3% 的键被退格修正（归因口径）
        backspaces = counts.get(0x0E, 0) + counts.get(0x153, 0)
        corrected = 0
        for pid, press in counts.items():
            if pid in (0x0E, 0x153):
                continue
            errors = int(press * random.gauss(0.035, 0.012))
            if errors > 0:
                counts[pid] = press
                cur.execute("INSERT INTO key_daily(day,pid,press,corr) VALUES(?,?,?,?) "
                            "ON CONFLICT(day,pid) DO UPDATE SET press=press+excluded.press, corr=corr+excluded.corr",
                            (julian, pid, press, min(errors, backspaces)))
                corrected += min(errors, backspaces)
            else:
                cur.execute("INSERT INTO key_daily(day,pid,press,corr) VALUES(?,?,?,0) "
                            "ON CONFLICT(day,pid) DO UPDATE SET press=press+excluded.press",
                            (julian, pid, press))
            cur.execute("INSERT INTO key_meta(pid,vk,first_day) VALUES(?,0,?) ON CONFLICT(pid) DO NOTHING", (pid, julian))

        # 时段分布：上午 9-12、下午 14-18 为高峰
        hours = [0] * 24
        for _ in range(day_total):
            hour = int(random.gauss(15, 4.5)) % 24
            hours[hour] += 1
        for hour, value in enumerate(hours):
            if value:
                cur.execute("INSERT INTO day_hour(day,hour,press,corr) VALUES(?,?,?,0) "
                            "ON CONFLICT(day,hour) DO UPDATE SET press=press+excluded.press",
                            (julian, hour, value))

        total += day_total

    cur.execute("INSERT INTO meta(k,v) VALUES('first_data_day',?) ON CONFLICT(k) DO UPDATE SET v=excluded.v",
                (str((first_day - date(1970, 1, 1)).days + JULIAN_UNIX_EPOCH),))
    cur.execute("INSERT INTO meta(k,v) VALUES('demo_data','1') ON CONFLICT(k) DO UPDATE SET v=excluded.v")
    con.commit()
    con.close()

    print("已写入 %d 天、约 %d 次键击的演示数据 -> %s" % (args.days, total, args.db))
    return 0


if __name__ == "__main__":
    sys.exit(main())
