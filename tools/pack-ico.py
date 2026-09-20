"""keyCounter - 把 icon-gen 产出的多尺寸 PNG 打包成 Windows .ico。

用法： python tools/pack-ico.py <png 目录> <输出 .ico>
.ico 允许每个条目直接存放 PNG 数据（Vista+ 支持），因此不需要额外的图像库。
"""
import struct
import sys
from pathlib import Path

SIZES = [16, 24, 32, 48, 64, 128, 256]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])

    images = []
    for size in SIZES:
        path = src / ("app-%d.png" % size)
        if not path.exists():
            print("警告：缺少 %s，跳过" % path)
            continue
        images.append((size, path.read_bytes()))
    if not images:
        print("没有可打包的 PNG")
        return 1

    header = struct.pack("<HHH", 0, 1, len(images))  # reserved, type=icon, count
    offset = len(header) + 16 * len(images)
    entries = b""
    payload = b""
    for size, data in images:
        dim = 0 if size >= 256 else size  # 256 用 0 表示
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        payload += data

    dst.write_bytes(header + entries + payload)
    print("已生成 %s（%d 个尺寸：%s）" % (dst, len(images), ", ".join(str(s) for s, _ in images)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
