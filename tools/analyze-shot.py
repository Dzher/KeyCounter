"""keyCounter - 开发用：对截图做像素级校验（无 GUI 环境下检查渲染结果）。

用法： python tools/analyze-shot.py shot.png
"""
import sys
from collections import Counter

from PIL import Image

# 深色主题关键色
THEME = {
    "window": (0x0F, 0x11, 0x16),
    "surface": (0x17, 0x1A, 0x21),
    "surfaceAlt": (0x1D, 0x21, 0x29),
    "keycap": (0x23, 0x26, 0x2E),
    "keycapEdge": (0x2E, 0x32, 0x3C),
    "text": (0xE6, 0xE9, 0xF0),
}
# Ember 色带端点
RAMP = {
    "deep-blue": (0x12, 0x3A, 0x5E),
    "cyan": (0x1C, 0x7F, 0xA8),
    "green": (0x2B, 0xB6, 0x73),
    "yellow-green": (0xC9, 0xD9, 0x3B),
    "orange": (0xF2, 0xA9, 0x3B),
    "red": (0xE5, 0x48, 0x4D),
    "deep-red": (0x8C, 0x1C, 0x2B),
}


def near(pixel, color, tolerance=26):
    return all(abs(int(pixel[i]) - color[i]) <= tolerance for i in range(3))


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "shot.png"
    image = Image.open(path).convert("RGB")
    width, height = image.size
    pixels = list(image.getdata())
    print("图像：%s  %dx%d" % (path, width, height))

    unique = Counter(pixels)
    print("不同颜色数：%d" % len(unique))
    print("出现最多的 12 种颜色：")
    for color, count in unique.most_common(12):
        print("   #%02X%02X%02X  %8d  %5.2f%%" % (color[0], color[1], color[2], count, 100.0 * count / len(pixels)))

    print("\n主题色命中：")
    for name, color in THEME.items():
        hits = sum(count for pixel, count in unique.items() if near(pixel, color))
        print("   %-12s #%02X%02X%02X  %8d 像素" % (name, *color, hits))

    print("\n热力色带命中（含包围盒）：")
    for name, color in RAMP.items():
        xs, ys, hits = [], [], 0
        for index, pixel in enumerate(pixels):
            if near(pixel, color):
                hits += 1
                xs.append(index % width)
                ys.append(index // width)
        if hits:
            print("   %-12s #%02X%02X%02X  %7d 像素  区域 x[%d..%d] y[%d..%d]"
                  % (name, *color, hits, min(xs), max(xs), min(ys), max(ys)))
        else:
            print("   %-12s #%02X%02X%02X        0 像素" % (name, *color))

    # 键盘区域（中左卡片）最热的颜色：找红色系像素最密集的一条水平带
    red_like = [(index % width, index // width) for index, pixel in enumerate(pixels)
                if pixel[0] > 150 and pixel[0] > pixel[1] + 40 and pixel[0] > pixel[2] + 40]
    print("\n偏红像素（含强调色/警示色）：%d" % len(red_like))
    if red_like:
        print("   红色像素分布 y[%d..%d] x[%d..%d]"
              % (min(y for _, y in red_like), max(y for _, y in red_like),
                 min(x for x, _ in red_like), max(x for x, _ in red_like)))

    # 非背景像素占比（判断是否有内容渲染出来）
    background = THEME["window"]
    content = sum(count for pixel, count in unique.items() if not near(pixel, background, 6))
    print("\n非窗口底色像素占比：%.1f%%" % (100.0 * content / len(pixels)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
