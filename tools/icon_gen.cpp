// keyCounter - 图标生成器：把应用图标渲染成多尺寸 PNG（供 pack-ico.py 打包为 .ico）
// 用法： keyCounterIconGen <输出目录>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QDir>
#include <QTextStream>

#include "app/Icons.h"

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);

    const QString dir = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    if (!QDir().mkpath(dir))
    {
        out << QStringLiteral("无法创建目录：%1\n").arg(dir);
        return 1;
    }

    const QColor accent(0x4C, 0x8D, 0xFF);
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    for (int size : sizes)
    {
        const QPixmap pixmap = kc::icons::appIcon(accent, size);
        const QString path = QStringLiteral("%1/app-%2.png").arg(dir).arg(size);
        if (!pixmap.save(path, "PNG"))
        {
            out << QStringLiteral("保存失败：%1\n").arg(path);
            return 1;
        }
        out << QStringLiteral("生成 %1\n").arg(path);
    }
    out.flush();
    return 0;
}
