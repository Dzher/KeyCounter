"""keyCounter - 直接读取统计数据库的开发校验脚本。

用法： python tools/check-db.py [db 路径]
默认读取 %APPDATA%/keyCounter/keycounter.db（只读方式打开，不影响正在运行的程序）。
"""
import os
import sqlite3
import sys
from datetime import date, timedelta

JULIAN_UNIX_EPOCH = 2440588


def julian_to_date(day):
    return date(1970, 1, 1) + timedelta(days=day - JULIAN_UNIX_EPOCH)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.environ.get("APPDATA", ""), "keyCounter", "keycounter.db")
    if not os.path.exists(path):
        print("数据库不存在：%s" % path)
        return 1

    uri = "file:%s?mode=ro" % path.replace("\\", "/")
    con = sqlite3.connect(uri, uri=True)
    cur = con.cursor()

    print("数据库：%s" % path)
    print("表：%s" % [r[0] for r in cur.execute(
        "select name from sqlite_master where type='table' order by name")])
    print("meta：%s" % cur.execute("select k, v from meta order by k").fetchall())
    print()

    rows = cur.execute("select day, pid, press, corr from key_daily order by day, press desc").fetchall()
    for day, pid, press, corr in rows[:40]:
        print("  %s  pid=0x%03X  按下=%-6d 打错修正=%d" % (julian_to_date(day), pid, press, corr))
    print("key_daily 行数：%d" % len(rows))

    hours = cur.execute("select hour, sum(press) from day_hour group by hour order by hour").fetchall()
    print("时段分布：%s" % hours)
    print("已登记的键：%d 个" % cur.execute("select count(*) from key_meta").fetchone()[0])
    con.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
