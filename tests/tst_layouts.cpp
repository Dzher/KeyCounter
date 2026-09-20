#include <QTest>

#include "core/KeyCatalog.h"
#include "layout/LayoutModel.h"

using namespace kc;

class TestLayouts : public QObject
{
    Q_OBJECT
private slots:
    void builtinLayoutsAreValid();
    void ansiGeometry();
    void tklGeometry();
    void compactGeometry();
};

void TestLayouts::builtinLayoutsAreValid()
{
    LayoutRepository& repo = LayoutRepository::instance();
    const QStringList ids = repo.availableIds();
    QCOMPARE(ids.size(), 3);

    for (const QString& id : ids)
    {
        const auto layout = repo.layout(id);
        QVERIFY2(layout != nullptr, qPrintable(id));
        QCOMPARE(layout->id, id);
        QVERIFY2(layout->validate().isEmpty(), qPrintable(id + QLatin1Char(' ') + layout->validate()));
        QVERIFY(layout->keys.size() > 60);
        QVERIFY(layout->widthU > 10.0);
        QVERIFY(layout->widthU < 30.0);
        QVERIFY(layout->heightU >= 5.0);
        QVERIFY(layout->gapU > 0.0 && layout->gapU < 0.2);

        // 主键区的核心键必须在场且可统计
        for (quint16 pid : {pk::A, pk::Space, pk::LeftShift, pk::Enter, pk::Backspace, pk::Escape})
        {
            const KeyGeometry* key = layout->find(pid);
            QVERIFY2(key != nullptr, qPrintable(QStringLiteral("%1 缺少 pid 0x%2").arg(id).arg(pid, 3, 16, QLatin1Char('0'))));
            QVERIFY(key->countable);
        }
    }

    // 未知布局回退到默认
    QVERIFY(repo.layout(QStringLiteral("does-not-exist")) != nullptr);
    QCOMPARE(repo.layout(QStringLiteral("does-not-exist"))->id, LayoutRepository::defaultLayoutId());
}

void TestLayouts::ansiGeometry()
{
    const auto layout = LayoutRepository::instance().layout(QStringLiteral("ansi-104"));
    QCOMPARE(layout->keys.size(), qsizetype(104));
    QCOMPARE(layout->heightU, 6.0);
    QCOMPARE(layout->widthU, 22.5);

    // 左右修饰键必须是不同统计键
    QVERIFY(layout->find(pk::LeftShift) != nullptr);
    QVERIFY(layout->find(pk::RightShift) != nullptr);
    QVERIFY(layout->find(pk::LeftShift)->cell.x() < layout->find(pk::RightShift)->cell.x());

    // 小键盘回车独立于主回车
    QVERIFY(layout->find(pk::NumpadEnter) != nullptr);
    QVERIFY(layout->find(pk::Enter) != nullptr);
    QVERIFY(layout->find(pk::Enter)->cluster != layout->find(pk::NumpadEnter)->cluster);

    // 空格键居中于主键区
    const KeyGeometry* space = layout->find(pk::Space);
    QCOMPARE(space->cell.width(), 6.25);

    // 数字小键盘 + 键跨两行
    QCOMPARE(layout->find(pk::NumpadAdd)->cell.height(), 2.0);
    QCOMPARE(layout->find(pk::Numpad0)->cell.width(), 2.0);

    // 所有可统计键的 pid 都在 KeyCatalog 中已知（避免布局与目录脱节）
    for (const KeyGeometry& key : layout->keys)
    {
        if (!key.countable || key.pid == kInvalidPid)
        {
            continue;
        }
        QVERIFY2(QString::fromLatin1(keyInfo(key.pid).name) != QStringLiteral("Unknown"),
                 qPrintable(QStringLiteral("布局中出现未登记 pid 0x%1").arg(key.pid, 3, 16, QLatin1Char('0'))));
    }
}

void TestLayouts::tklGeometry()
{
    const auto layout = LayoutRepository::instance().layout(QStringLiteral("tkl-87"));
    QCOMPARE(layout->keys.size(), qsizetype(87));
    QCOMPARE(layout->widthU, 18.25);
    QVERIFY(layout->find(pk::Numpad0) == nullptr);
    QVERIFY(layout->find(pk::NumpadEnter) == nullptr);
    QVERIFY(layout->find(pk::ArrowUp) != nullptr);
}

void TestLayouts::compactGeometry()
{
    const auto layout = LayoutRepository::instance().layout(QStringLiteral("compact-68"));
    QVERIFY(layout->keys.size() >= 60);
    QCOMPARE(layout->widthU, 16.25);

    // 无 F 区、无数字小键盘
    QVERIFY(layout->find(pk::F1) == nullptr);
    QVERIFY(layout->find(pk::Numpad1) == nullptr);

    // 有方向键，且 ↑ 位于 ↓ 正上方
    const KeyGeometry* up = layout->find(pk::ArrowUp);
    const KeyGeometry* down = layout->find(pk::ArrowDown);
    QVERIFY(up != nullptr && down != nullptr);
    QCOMPARE(up->cell.x(), down->cell.x());
    QCOMPARE(down->cell.y() - up->cell.y(), 1.0);

    // Fn 键存在但不可统计
    bool hasFn = false;
    for (const KeyGeometry& key : layout->keys)
    {
        if (key.label == QStringLiteral("Fn"))
        {
            hasFn = true;
            QVERIFY(!key.countable);
        }
    }
    QVERIFY(hasFn);
}

QTEST_GUILESS_MAIN(TestLayouts)

#include "tst_layouts.moc"
