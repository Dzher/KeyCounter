# keyCounter 技术方案

文档版本：v1.0（待评审） · 目标平台：Windows 10/11 x64 · 工具链：Qt 6.8.2 `msvc2022_64` + MSVC 2022 + CMake + Ninja

---

## 0. 已确认的决策（评审输入）

| 决策点 | 结论 | 影响 |
|--------|------|------|
| UI 技术栈 | Qt Widgets + 全自绘（QPainter） | 内存 25–40 MB，启动 <300 ms，热力图/图表全部自绘，无 QML 运行时 |
| 存储 | SQLite（Qt 自带 QSQLITE）+ WAL + 批量 upsert | 崩溃安全、易扩展；内存增量累加，15 s / 退出 / 跨零点时落盘 |
| 键盘布局 | 内置 104 ANSI、87 TKL、68 小配列，三套可切换 | 布局为声明式 JSON 数据，后续可加自定义布局 |
| 开机自启 | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` + `--silent` | 免管理员；局限：以普通权限运行，无法统计提权窗口的按键（见 §4.3） |
| 全局快捷键 | `RegisterHotKey`，**默认关闭**，组合键在设置中由用户自定义 | 例如 `Ctrl+Alt+K` 唤起面板；注册失败提示冲突，不影响其他功能（见 §9.4） |
| 导出 / 安装包 | **本期搁置** | 仅交付免安装绿色目录；导出接口位置预留 |

---

## 1. 目标与非目标

### 1.1 目标

- **准**：逐物理键精确计数（区分左右修饰键、主回车/小键盘回车），自动重复（长按）不算多次按下。
- **轻**：常驻后台。空闲 CPU ≈ 0%（事件驱动，无轮询）；打字时热路径 < 2 µs/键、无锁、无堆分配；常驻内存 < 45 MB。
- **稳**：断电/崩溃最多丢最后 15 s 的计数；数据库损坏时可自动隔离重建，不静默清零。
- **美**：热力虚拟键盘与统计图表自绘，深色主题、圆角卡片、平滑色带、悬停反馈，缩放（DPI）下依旧锐利。
- **净**：不引入第三方库；不记录按键序列与窗口标题；不联网。
- **可控**：托盘随手进入；可选全局快捷键唤起面板（默认关闭）；随时暂停统计。

### 1.2 非目标（v1 明确不做，列入扩展点）

- 不记录按键序列 / 击键内容 / 窗口标题（隐私红线）。
- 不做云同步、账号、上报。
- 不做 macOS/Linux 支持（采集层为 Win32 Raw Input；接口保留抽象）。
- 不做按应用统计（`app_daily` 已在 schema 中预留扩展位，v1.1 视需求加入，需抓前台窗口，成本与隐私权衡后再说）。
- 不做数据导出（CSV / PNG）、不做安装包（NSIS/MSI）：**本期搁置**，交付免安装绿色目录；导出接口位置预留（`StatsStore::exportCsv()` / `KeyboardHeatmapWidget::renderToImage()`）。

---

## 2. 技术选型总览

| 关注点 | 选型 | 备选与否决理由 |
|--------|------|---------------|
| 按键采集 | **Raw Input**（`RegisterRawInputDevices`，`RIDEV_INPUTSINK`） | 否决 `WH_KEYBOARD_LL`：全局钩子会进入系统按键处理链，每个按键都要跨进程投递消息；回调超时（默认 `LowLevelHooksTimeout`=300 ms）会被系统**静默摘钩**；且更容易被安全软件/反作弊标记。Raw Input 不注入、不挂钩，数据走本进程消息队列，失焦也能收到。 |
| 数据落盘 | **SQLite/QSQLITE**（qtbase 自带驱动，非第三方） | 否决纯二进制自研格式：省下的资源微乎其微，却要自研一致性/迁移/查询。否决 JSON 快照：崩溃会丢一个刷新周期且有整文件重写。 |
| UI | **Qt Widgets + QPainter 自绘** | 否决 QML：视觉上限更高但常驻内存 80–150 MB、启动 ~1 s，与"常驻低占用"冲突。 |
| 图表 | 自绘（QPainter + 预渲染 pixmap 缓存） | 否决 Qt Charts：模块体积大、样式定制受限，且我们只需柱状/折线/条形三种。 |
| 自绘色彩插值 | 自实现 OKLab 插值（约 30 行数学） | 感知均匀的色带过渡，比 sRGB 线性插值明显更平滑，且零依赖。 |
| 配置 | `QSettings`（HKCU\Software\keyCounter） | 轻量、免文件解析；数据仅十余项。 |

---

## 3. 系统架构

单进程、单可执行文件。**三个线程**：GUI 线程（含采集窗口）、存储线程（SQLite）、（可选）统计计算线程。

```
                  Windows Kernel
                        │  WM_INPUT  (Raw Input, RIDEV_INPUTSINK)
                        ▼
┌───────────────────────── GUI 线程（Qt 主线程） ─────────────────────────┐
│  RawInputWindow(消息窗口) ──► KeyEventPump                              │
│        │  解析 RAWKEYBOARD：物理键 id / 按下-抬起 / 时间戳              │
│        ▼                                                                │
│  CounterEngine（热路径：定长数组累加，无锁无分配）                      │
│        ├─ CorrectionTracker（删除 → 归因到刚打错的键）                  │
│        └─ DirtyBatch（定长行缓冲，满/超时 → 移交存储线程）              │
│                                                                         │
│  MainWindow ──► KeyboardHeatmapWidget / TrendChartWidget / Cards / Top  │
│        ▲                ▲                                               │
│        └── StatsQuery ──┘（范围查询 + LRU 缓存 + 版本号失效）           │
│  TrayController / SettingsDialog / AutoStart / SingleInstance           │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │ 队列信号（行批量，move 语义）
                                 ▼
┌────────────────────── 存储线程 QThread ──────────────────────┐
│  StatsStore：WAL、预编译 upsert、单事务提交、quick_check、备份 │
└───────────────────────────────────────────────────────────────┘
```

**职责边界**

- `KeyEventPump` 只做「字节 → 结构化事件」，不含业务。
- `CounterEngine` 是唯一的写入口：内存聚合 + 纠错归因 + 脏批次管理，运行在 GUI 线程但**不做任何 I/O、不分配内存、不加锁**。
- `StatsStore` 是唯一的 DB 写入口，运行在独立线程；只接受"行批量"，不感知业务。
- `StatsQuery` 是唯一的 DB 读入口，在 GUI 线程同步查询（数据量极小，见 §11），带 LRU 缓存。
- UI 层只订阅 `StatsQuery` 的结果与 `CounterEngine` 的"今日总数变化"信号。

---

## 4. 输入采集层

### 4.1 采集通道

```cpp
RAWINPUTDEVICE rid{};
rid.usUsagePage = 0x01;          // Generic Desktop
rid.usUsage     = 0x06;          // Keyboard
rid.dwFlags     = RIDEV_INPUTSINK;  // 即使本进程无焦点也接收
rid.hwndTarget  = msgWindow;     // 专用消息窗口（HWND_MESSAGE 不可用于 Raw Input，需普通不可见窗口）
RegisterRawInputDevices(&rid, 1, sizeof(rid));
```

- 消息窗口用 `WS_POPUP`、0×0、不显示的 `QWindow`（`createWindowContainer` 不需要，直接 `CreateWindowExW`）或 `QAbstractNativeEventFilter` 挂在 `QApplication` 上。**选后者**：`QAbstractNativeEventFilter::nativeEventFilter` 里处理 `WM_INPUT`，无需自建窗口类（仍在主线程消息循环内，开销为零）。
- `WM_INPUT` 处理：`GetRawInputData(hRawInput, RID_INPUT, stackBuf, &size, sizeof(RAWINPUTHEADER))`，缓冲区固定 `sizeof(RAWINPUT)`（64 B）栈上分配，无堆分配。

> 注意：`RIDEV_INPUTSINK` 要求 `hwndTarget` 是真实窗口句柄；故在 `QApplication` 之外还需一个不可见的 `CreateWindowExW(L"STATIC", ...)` 消息窗口作为目标。设计上使用一个约 20 行的 `RawInputWindow` 封装（创建/销毁/过滤回调），不引入额外线程。

### 4.2 事件解析与物理键标识

`RAWKEYBOARD` 提供：

| 字段 | 用法 |
|------|------|
| `MakeCode` | 扫描码（scan code set 1），**不含** E0/E1 前缀 |
| `Flags` | `RI_KEY_BREAK`=抬起；`RI_KEY_E0`/`RI_KEY_E1`=前缀 |
| `VKey` | 虚拟键；`0xFF`（`VK__none_`）表示无效/伪键 |
| `Message` | 可忽略（以 Flags 为准） |

**物理键 id（`pid`，统计主键）**

```
pid = MakeCode | (E0 ? 0x100 : 0) | (E1 ? 0x200 : 0)      // < 0x400，u16 存储
```

- 用 pid 而不用 VK：VK 会随输入法/键盘布局变化（同一物理键在中英/法语布局下 VK 不同），而用户关心的是"**哪个物理键被按了多少次**"。
- 天然区分：左 Shift `0x02A` / 右 Shift `0x136`、左 Ctrl `0x01D` / 右 Ctrl `0x11D`、主回车 `0x01C` / 小键盘回车 `0x11C`、`/` 与 小键盘 `/`、方向键（E0 前缀）等。
- VK 仍然记录：仅在"首次见到该 pid"时写入 `key_meta`，用于导出/诊断，不参与统计主键。

**必须过滤的伪事件**

| 情况 | 处理 |
|------|------|
| `VKey == 0xFF`（`VK__none_`） | 丢弃 |
| `MakeCode == 0xFF`（`KEYBOARD_OVERRUN_MAKE_CODE`） | 丢弃 |
| PrintScreen 的伪 Shift：`RI_KEY_E0` 且 `MakeCode ∈ {0x2A, 0x36}` | 丢弃（真实的左/右 Shift 不带 E0） |
| Pause/Break：E1 前缀 + VKey=VK_PAUSE | 保留（pid=0x245） |

**自动重复（长按）**

Raw Input 对长按会连续投递 make 码。用 64 字节的按下位图（`std::bitset<1024>` 或 `uint64_t[16]`）判定"下降沿"：

- `make` 且该位为 0 → 计数 +1，置位；
- `make` 且该位为 1 → 自动重复，**不计数**；
- `break` → 清位。
- 窗口失焦/会话切换时不做特殊处理（Raw Input 的 break 事件不会丢）；进程启动时位图清零，避免"启动时按住的键永远不计数"的边界问题。

### 4.3 已知限制（写进"关于/帮助"页，避免用户困惑）

| 限制 | 原因 | 缓解 |
|------|------|------|
| 提权窗口（以管理员运行的 CMD/PowerShell/任务管理器等）中的按键统计不到 | UIPI：高完整性进程的前台输入不投递给普通完整性进程 | 文档说明；v1.1 可选「任务计划程序 + 最高权限」方案 |
| UAC 弹窗、锁屏界面、Winlogon 桌面 | 安全桌面不投递 | 文档说明 |
| Fn 键 | 由键盘硬件/EC 处理，不产生系统键码 | 布局中标注为"不可统计"（灰色不可点亮） |
| `SendInput` 注入的按键会被计入 | Raw Input 无法区分注入与真实 | 可接受（本人一般不注入）；单元测试改用 F13–F24 虚拟键 |
| 某些独占 DirectInput 游戏 | 极少数情况按键被独占 | 观察后如确有问题，v1.1 提供"I/O 兼容模式"（回退 `WH_KEYBOARD_LL`）开关 |
| 以管理员运行的多用户/多会话 | 每个会话各起一个实例，数据按下 `RDP-<sessionId>` 分区？ | v1 不处理；单实例锁按 SID 隔离，避免多用户互踢 |

---

## 5. 统计内核（热路径设计）

### 5.1 数据结构

```cpp
struct DayKeyBatch {                       // 热路径操作的最小单元
    uint32_t day;                          // QDate::toJulianDay()，本地日历日
    std::array<uint32_t, kMaxPid> press;   // 1024 * 4 B = 4 KB
    std::array<uint32_t, kMaxPid> corr;    // 4 KB
    std::array<uint32_t, 24>     hour;     // 时段分布
};
```

- 内存中只有**当前这一天**的批次（8 KB，常驻 L1/L2）。
- 跨零点（或系统时钟回拨/时区变化导致的日期变化）时：把当前批次交给存储线程，清空，用新的 `day` 继续。

### 5.2 单键处理流程（伪代码，目标 < 2 µs）

```
onRawKey(makeCode, flags, vk):
    pid = encode(makeCode, flags)          # 位运算
    down = !(flags & BREAK)
    if !down: clearBit(pid); return
    if testAndSetBit(pid): return          # 自动重复，丢弃
    if paused: return                      # 暂停期间完全丢弃（含纠错）
    now = fastTick()                       # GetTickCount64 / QElapsedTimer，不取日历时间
    if now > dayDeadline: rolloverDay()    # 每 ~2^31 次才走一次，见下
    press[pid]++
    hour[localHourCache]++
    tracker.onKey(pid)
```

- **日期开销摊销**：日历换算（本地时区、夏令时）只在 `dayDeadline`（下一个本地零点的时间戳，秒级）到期时计算一次；其余按键只做一次整数比较。系统时钟被改时，`dayDeadline` 失效由 1 Hz 的"轻量守护定时器"（仅在脏数据存在时运行）兜底。
- **纠错归因**（`CorrectionTracker`，需求 5）：

  ```
  维护一个小环形缓冲 recent[32]（仅内存，绝不落盘、绝不写日志）：
    - 可归因按键（字母/数字/符号/空格）= 入栈
    - 修饰键（Shift/Ctrl/Alt/Win） = 忽略（不打断）
    - Enter/Esc/Tab/方向/Home/End/PgUp/PgDn/鼠标点击不可见但可用超时兜底 = 清空缓冲
    - 超过 10 s 未输入 = 视为新语境，缓冲失效
  onKey(Backspace 或 Delete):
    press[pid]++                       # 删除键本身也计入按下次数（字面口径）
    if recent 非空: corr[recent.pop()]++   # 归因口径：刚打错的键 +1
  ```
  - 连续退格 N 次即 N 个错误；一次删除只归因一个键。
  - 用户可切换**字面口径**（错误数 = 删除键按下次数）与**归因口径**（哪个键最常打错），两者都从同一份数据派生（见 §6.1）。

### 5.3 脏数据移交（无阻塞、无扩容风险）

- `DirtyBatch` 就是 §5.1 的结构本身，`dirty` 标志由按键置位。
- 移交策略（单次定时器，非周期轮询）：
  1. 第一个脏按键 → 启动 15 s 单次定时器（`Qt::VeryCoarseTimer`，降低唤醒成本）；
  2. 定时器到期或跨零点或收到 `WM_POWERBROADCAST`/`WM_WTSSESSION_CHANGE`（休眠/锁屏）或退出 → `emit flushRequested(batch)`，用 `move` 语义传给存储线程，然后清空本地批次；
  3. 若上一次 flush 尚未完成（存储线程忙），在内存里与新批次**合并**（累加）后重试，绝不阻塞/丢数据。

> 稳定性：批次与合并逻辑都是定长数组累加，长时间运行不会增长内存；不存在无界队列。

---

## 6. 存储层

### 6.1 Schema（v1）

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous  = NORMAL;
PRAGMA foreign_keys = ON;

-- 逐日逐键聚合。day = 本地日历日的 Julian Day（QDate::toJulianDay）
-- pid = (E1<<9)|(E0<<8)|makeCode
CREATE TABLE IF NOT EXISTS key_daily (
    day   INTEGER NOT NULL,
    pid   INTEGER NOT NULL,
    press INTEGER NOT NULL,      -- 按下次数（不含自动重复）
    corr  INTEGER NOT NULL,      -- 归因错误次数（被 Backspace/Delete 修正）
    PRIMARY KEY (day, pid)
) WITHOUT ROWID;                 -- 按 (day,pid) 聚簇，范围查询即连续扫描

-- 时段分布（每天 24 行）
CREATE TABLE IF NOT EXISTS day_hour (
    day   INTEGER NOT NULL,
    hour  INTEGER NOT NULL,
    press INTEGER NOT NULL,
    corr  INTEGER NOT NULL,
    PRIMARY KEY (day, hour)
) WITHOUT ROWID;

-- 首次见到某键时的元信息（诊断/导出用）
CREATE TABLE IF NOT EXISTS key_meta (
    pid   INTEGER PRIMARY KEY,
    vk    INTEGER,
    first_day INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v TEXT);
-- meta: install_day, first_data_day, active_days, layout_id, app_version
```

- **数据量**：约 120 pid/天 → 每天 ≤ 120 + 24 行；一年约 5 万行 ≈ **1.5–2 MB**（含 WAL 与索引）。保留策略：不采样、不降采样，长期保留（可选"仅保留最近 N 年"设置）。
- **写入**：预编译单条 upsert，单事务批量提交：

  ```sql
  INSERT INTO key_daily(day,pid,press,corr) VALUES(?1,?2,?3,?4)
  ON CONFLICT(day,pid) DO UPDATE SET press = press + excluded.press,
                                     corr  = corr  + excluded.corr;
  ```
  每 15 s 最多 120 条 upsert，年写入量 < 1 万事务，I/O 压力可忽略；WAL 使读（UI 查询）与写互不阻塞。
- **错误口径的双视角**（都能由同一份数据派生，无需冗余列）：
  - 字面口径：`SUM(press) WHERE pid = Backspace.pid OR pid = Delete.pid`；
  - 归因口径：`SUM(corr) GROUP BY pid`（排除删除键自身）。

### 6.2 查询

| 视图 | 区间（本地日历，含首尾） | SQL |
|------|--------------------------|-----|
| 今天 | `[today, today]` | `SELECT pid,SUM(press),SUM(corr) FROM key_daily WHERE day BETWEEN ?1 AND ?2 GROUP BY pid` |
| 本周 | 周一（可配置为周日）~ 周日 | 同上 |
| 本月/今年 | 自然月/年的首尾（为了图表对齐保留未来空槽） | 同上 |
| 累计 | `[first_data_day, today]` | 同上 |
| 趋势 | 同上区间 | `SELECT day,SUM(press),SUM(corr) ... GROUP BY day ORDER BY day`（稀疏，内存补零） |
| 时段 | 同上区间 | `SELECT hour,SUM(press) FROM day_hour ... GROUP BY hour` |

- 一年期的聚合扫描 ≈ 5 万行 → 在本机（NVMe + 页缓存）实测目标 **< 15 ms**；因此 UI 线程同步查询是可接受的。
- **LRU 缓存**：`QCache<RangeKey, PeriodStats>`（容量 8）。`RangeKey = (startDay, endDay)`；全局 `revision` 在每次落盘后 +1，命中但版本过期则仅失效"包含当前天"的区间（历史区间数据不会再变，缓存长期有效）。

### 6.3 文件位置与健壮性

| 项 | 方案 |
|----|------|
| DB 路径 | 默认 `%APPDATA%\keyCounter\keycounter.db`（`QStandardPaths::AppDataLocation`）；若 exe 同级存在 `portable.flag` → 便携模式，数据放 exe 同级 `data\`；用户可在设置中指定任意数据目录（`data/directory` 存注册表），更改时自动迁移数据库 + WAL/SHM + `backup\`，失败回滚并继续用旧目录 |
| 启动自检 | `PRAGMA quick_check`（小库 < 5 ms）；失败 → 将库重命名为 `keycounter.corrupt-<yyyymmdd-HHmmss>.db` 并新建，托盘气泡提示（**绝不静默丢数据**） |
| 备份 | 每次会话首个 flush 前，若当天尚未备份 → 复制到 `backup\keycounter-<yyyymmdd>.db`，保留最近 7 份（可关闭） |
| 迁移 | `PRAGMA user_version` 递增；v0→v1 建表；后续 `ALTER TABLE` 逐步迁移，全程单事务 |
| 时钟异常 | 记录 `last_day`；新 `day < last_day` 时照常按新日期计数，但写 `meta.clock_anomaly` 计数（防误判为数据损坏） |

---

## 7. 键盘布局引擎

需求 4「虚拟键盘要和真实键盘布局一致」→ 布局必须**声明式、可切换、可扩展**，渲染按等比例（1U = 标准键宽）缩放。

### 7.1 数据模型

```jsonc
// resources/layouts/ansi-104.json（内置，qrc；用户自定义放 %APPDATA%\keyCounter\layouts\*.json）
{
  "id": "ansi-104", "name": "104 键全尺寸 (ANSI)", "unitGap": 0.06,
  "rows": [
    { "keys": [
      { "pid": 41, "label": "Esc", "sub": "", "x": 0,  "y": 0, "w": 1 },
      { "pid": 2,  "label": "1",   "sub": "!", "x": 2,  "y": 0, "w": 1 },
      { "pid": 14, "label": "Bksp","sub": "", "x": 13.25, "y": 0, "w": 2, "group": "main" }
      // ...
    ]},
    { "keys": [ { "pid": 82, "label": "↑", "x": 15.25, "y": 4, "w": 1, "group": "nav" } ] }
  ],
  "clusters": [ {"id":"main","title":"主键区"}, {"id":"nav","title":"编辑/方向"},
                {"id":"numpad","title":"数字小键盘"}, {"id":"fn","title":"功能区"} ]
}
```

要点：
- `pid` 与 §4.2 编码一致 → **按键计数直接落到布局单元**，无需中间映射表。
- `label`/`sub` 由布局数据给出（而非 `MapVirtualKey`），因此虚拟键盘的字样永远和用户实体键盘一致（US 布局的 `1/!`、中文键盘的刻印、日文键盘的假名等）。
- 坐标以 U 为单位（浮点），支持 1.25/1.5/1.75/2/2.25/6.25U 等非标准宽度与行偏移（如 `Bksp` 前的 `\` 的 0.5U 错位、68 配列的方向键嵌入）。
- 缺 `pid` 的键（如 Fn、品牌功能键）仍渲染，但标记 `"countable": false`（灰阶、不可点亮）。

**内置三套**（用户已确认）：

| id | 名称 | 规模 | 说明 |
|----|------|------|------|
| `ansi-104` | 104 全尺寸 ANSI | 主键区 + 编辑簇 + 方向键 + 数字小键盘 | 全尺寸键盘/外接键盘主流 |
| `tkl-87` | 87 键 TKL | 主键区 + 编辑簇 + 方向键 | 去小键盘；数字输入统计口径变小 |
| `compact-68` | 68 键小配列 | 主键区 + 方向键 + 少量编辑键 | 无 F 区/无小键盘；`Fn` 标注不可统计 |

自动选择逻辑：首启动引导页（可视化预览选择）→ 记入设置；托盘「键盘布局」子菜单随时切换；切换后热力图立即重排，已有计数不丢失（数据以 pid 存储，与布局无关）。

### 7.2 渲染管线（性能关键）

```
LayoutModel ──(布局/尺寸变化)──► KeyGeometry[]  (每个键的 QRectF, 单位: 逻辑像素)
                                      │
                      预渲染静态层 QPixmap：键帽底、边框、内高光、标签
                                      │
            每帧只做：热力填充(每键 1 次 roundedRect + 1 次渐变) ──► 合成
```

- **静态层缓存**：键帽底/标签在尺寸或主题变化时才重绘到 `QPixmap`（带 `devicePixelRatio`，避免缩放模糊）；热力变化只重绘填充。
- **重绘节流**：数据变化以 4 Hz 上限合并（`QTimer` 合并 + `update()`），不做逐键实时闪烁；提供「实时脉冲」开关（按下瞬间该键高亮环，同样 4 Hz 上限）。
- 缩放：`scale = (width - 2*pad - gaps) / layout.widthU`，X/Y 同尺度以保持键帽比例；窗口极小时优先保证可读性（最小键宽 24 px），超出则出现滚动。
- 字号自适应：`fontPx = clamp(keyH * 0.32, 9, 22)`；键宽 < 1.25U 时隐藏计数、只留标签；悬停显示 tooltip。

### 7.3 热力色带（满足需求 3：红色及更深 = 高频，无色 = 未按过）

- **归一化**：`t = clamp(log1p(count) / log1p(p97), 0, 1)`。对数刻度是必须的（空格/字母可达数千，F 键可能只有个位数，线性映射会让 90% 的键盘全灰）。`p97` = 当前周期内的 97 分位计数（回退策略：周期内总数为 0 时用 1，避免除零）。
- **色带**（默认 `Ember`，深色主题下的具体色标）：

  | t | 颜色 | 语义 |
  |---|------|------|
  | 0（未按过） | 键帽底色 `#23262E` + 边框 `#2E323C` | **无色** |
  | 0.00–0.12 | `#123A5E` 深蓝 | 极少 |
  | 0.12–0.30 | `#1C7FA8` 青 | 少 |
  | 0.30–0.50 | `#2BB673` 绿 | 中 |
  | 0.50–0.68 | `#C9D93B` 黄绿 | 偏多 |
  | 0.68–0.84 | `#F2A93B` 橙 | 多 |
  | 0.84–0.95 | `#E5484D` 红 | 很多 |
  | 0.95–1.00 | `#8C1C2B` 深红/酒红 | 最高频（"更深"） |

  - 插值在 **OKLab** 空间进行（sRGB→线性→LMS→OKLab），过渡无灰带；实现 ~30 行，无依赖。
  - 备选色带：`Inferno`（深红→猩红→橙→金）与 `Colorblind`（蓝→青→黄→红，红绿色盲友好），设置中切换。
- **可读性**：填充色与文字色对比度自动计算（相对亮度 Δ > 0.35 时用深色文字 `#0D0F14`，否则用 `#E6E9F0`）。
- **图例**：键盘下方渲染渐变条 + `0 / p50 / p97` 刻度 + 一句「对数刻度 · 数值越高越红」。
- **联动**：悬停键 → tooltip「A · 1,284 次 · 占比 12.4% · 错误 37 次」；点击键 → 下方折线切换为该键的日/周/月趋势（钻取）。

---

## 8. 界面设计（Qt Widgets 全自绘）

### 8.1 主窗口布局（默认 1080×760，记忆位置/大小）

```
┌ keyCounter ────────────────────────────────────────────────────────────────┐
│  ● 正在统计   今天 8,241 次键击              [今天][本周][本月][今年][累计] ⚙│  ← 顶部工具条（自绘分段控件）
├────────────────────────────────────────────────────────────────────────────┤
│ ┌──────────┐┌──────────┐┌──────────┐┌──────────┐                          │
│ │ 8,241    ││ 312      ││ Space    ││ 1,024    │   ← 4 张卡片（自绘圆角）  │
│ │ 本期键击 ││ 错误 3.8%││ 最高频键 ││ 日均     │                          │
│ └──────────┘└──────────┘└──────────┘└──────────┘                          │
│ ┌───────────────────────────────────────┐┌────────────────────────────────┐│
│ │           热力虚拟键盘（自绘）         ││ Top 12 高频键（横向条形，动画）││
│ │   ░░▒▒▓▓██ 按频率着色的键帽            ││ Space ████████████ 1,204       ││
│ │   [0 ────── 对数刻度 ────── p97]       ││ E     █████████ 912            ││
│ └───────────────────────────────────────┘└────────────────────────────────┘│
│ ┌────────────────────────────────────────────────────────────────────────┐ │
│ │ 趋势：近 30 天柱状（可切 90 天/12 月）+ 时段分布(24h) 子标签           │ │
│ └────────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 主题与视觉规范

| 元素 | 规范 |
|------|------|
| 主题 | 深色（默认）/ 浅色 / 跟随系统（`QStyleHints::colorScheme()`，Qt 6.5+） |
| 背景 | `#0F1116`；卡片 `#171A21`；卡片描边 `#242833`；圆角 12 px；卡片内边距 16 px |
| 阴影 | 用 1 px 描边 + 顶部 6% 白色渐变代替真实阴影（无 QGraphicsEffect，零性能代价） |
| 字体 | `Segoe UI Variable` → `Segoe UI` → 系统默认；数字使用 tabular figures（`QFont::setFeature`）保证跳动不抖动 |
| 强调色 | 默认使用系统的 Windows 强调色（`DwmGetColorizationColor`），失败回退 `#4C8DFF` |
| 动画 | 条形增长 180 ms `OutCubic`；周期切换卡片数字滚动 220 ms；全部基于 `QVariantAnimation`，可被"减少动画"设置关闭 |
| DPI | Qt 6 自动高 DPI + `PassThrough` 舍入策略；所有绘制用逻辑坐标；缓存 pixmap 携带 DPR |
| 空状态 | 无数据时键盘全灰 + 居中提示「开始打字即可统计 · 数据仅保存在本机」 |

### 8.3 托盘图标（自绘，动态）

- 图标：⌨️ emoji 键帽（Segoe UI Emoji 彩色渲染，与 README 标题同款；缺字体时回退手绘键帽）叠加一个**活动环**，环的填充比例 = 今日键击数 / 近 30 天日均（>100% 时环变红）；暂停时键帽转灰度、隐藏环。环的显示开关与固化颜色在 *设置 → 常规* 配置（`tray/ringEnabled`、`tray/ringColor`，空色 = 跟随主题强调色）；关环时键帽放大铺满画布。
- 更新节流：最多 1 次/分钟，且仅当数值区间变化（避免无谓重绘）；`QSystemTrayIcon::setToolTip` 显示「今日 8,241 次 · 错误 312」。
- 托盘菜单：`打开面板` / `暂停统计 ▸（1 小时 / 今日 / 直到恢复）` / `键盘布局 ▸（104 / 87 / 68）` / `开机自启 ✓` / `设置…` / `关于` / `退出`。
- 关闭主窗口 = 隐藏到托盘（首次弹气泡提示"仍在后台统计，右键托盘退出"）。

### 8.4 设置页（分页 4 组）

| 页 | 项 |
|----|----|
| 常规 | 开机静默自启（开关 + 当前注册表状态）、主题、色带、键盘布局、周起始日（周一/周日）、减少动画 |
| 快捷键 | 全局快捷键开关（**默认关**）+ 组合键自定义录制（`QKeySequenceEdit`）；拒绝无修饰键的组合；注册失败即提示被占用 |
| 统计 | 错误统计口径（归因/字面）、暂停状态显示、是否记录时段分布、数据保留年限 |
| 数据 | 数据目录（可打开）、清空数据（二次确认）、备份开关与份数（导出功能后续版本提供） |
| 关于 | 版本、Qt 版本、键盘布局来源、隐私声明、"已知限制"（§4.3） |

---

## 9. 生命周期与系统集成

### 9.1 启动流程

```
main()
 ├─ 解析参数：--silent（自启）/ --show / --reset-position
 ├─ QApplication + 高DPI策略 + 单实例检查（QLocalServer "keyCounter-<SID>"）
 │    └─ 已有实例 → 发送 "show" 并退出（自启时发送 "noop"）
 ├─ Settings 载入（QSettings HKCU\Software\keyCounter）
 ├─ StatsStore 打开 DB（quick_check / 迁移 / 备份），启动存储线程
 ├─ CounterEngine 初始化（今天批次、first_data_day、p97 缓存）
 ├─ RawInputWindow 注册（失败 → 托盘提示并降级为"仅 UI 模式"）
 ├─ TrayController 创建（首启动弹气泡）
 ├─ 自启自愈：若已开启自启且注册表路径 ≠ 当前 exe → 重写
 └─ 非 --silent 或首次运行 → 显示主窗口；否则仅托盘
```

### 9.2 事件与关机

| 事件 | 处理 |
|------|------|
| `WM_POWERBROADCAST` / `WM_WTSSESSION_CHANGE`（锁屏/休眠/切换用户） | 立即 flush + 清空按下位图 |
| 系统时钟/时区变化（`WM_TIMECHANGE`） | 重算 `dayDeadline`，必要时 rollover |
| 退出（托盘"退出" / `WM_QUERYENDSESSION`） | flush → `PRAGMA wal_checkpoint(TRUNCATE)` → 关闭 DB 线程 → 退出 |
| 崩溃 | 最多丢 15 s 计数；WAL 保证库不损坏 |

### 9.3 自启实现（选定：注册表 Run 键）

```
键值：HKCU\Software\Microsoft\Windows\CurrentVersion\Run
名称：keyCounter
数据："<exe 绝对路径>" --silent      ← 路径含空格时强制加引号
```

- 用 `QSettings(path, QSettings::NativeFormat)` 读写，无需额外 Win32 代码。
- 开关状态实时反映真实注册表值（而非本地布尔位），被安全软件/组策略删除后 UI 立即显示"未生效"。
- 每次启动自愈路径（便携版移动到新目录后自动修正）。
- v1.1 扩展（非本期）：`schtasks /Create /SC ONLOGON /RL HIGHEST` 以统计提权窗口按键，需一次 UAC 授权。

### 9.4 全局快捷键（可选，默认关闭）

- 实现：`RegisterHotKey(hwndRawInput, id, mods | MOD_NOREPEAT, vk)`，`WM_HOTKEY` 复用 §4.1 的隐藏消息窗口的 WndProc；`UnregisterHotKey` 在关闭开关、修改组合键、退出时调用（`WM_QUERYENDSESSION` 也会注销）。
- 默认值：开关 **off**，默认组合键预填 `Ctrl+Alt+K`（用户首次打开开关即生效，可再自定义）。
- 自定义：设置页用 `QKeySequenceEdit` 录制，保存前校验 —— 至少含一个修饰键（Ctrl/Alt/Shift/Win）且主键非纯修饰键；否则提示「全局快捷键必须包含修饰键」（避免吞掉单键影响正常打字）。
- 冲突处理：`RegisterHotKey` 返回 `FALSE`（`ERROR_HOTKEY_ALREADY_REGISTERED` = 1409）→ 设置页红字提示「该组合键已被其他程序占用」，并回退到上一个可用组合键；托盘菜单同步显示当前快捷键。
- 语义澄清：快捷键只在按下时唤起/隐藏面板，`MOD_NOREPEAT` 防止长按重复触发；被注册的组合键会从系统中被本进程截获（这是 `RegisterHotKey` 的固有行为，因此在 UI 上明确提示，且默认关闭）。

---

## 10. 性能预算与验证

| 指标 | 目标 | 验证方式 |
|------|------|---------|
| 空闲 CPU（0 键/分钟） | ≤ 0.05%（≈0） | 任务管理器 / ETW；确认无周期定时器（仅在脏数据存在时才有 1 个粗定时器） |
| 打字时 CPU（300 键/分钟） | < 0.3%（单核） | 注入压测 + `QueryProcessCycleTime` 采样 |
| 单键热路径 | < 2 µs（P99 < 10 µs） | `tests/bench_pump` 微基准（100 万次事件循环，无 I/O） |
| 常驻内存 | < 45 MB（工作集，Release，主窗口打开） | VMMap / `GetProcessMemoryInfo`；soak 测试 30 天无增长 |
| 启动到托盘 | < 300 ms | `--silent` 冷启动计时（含 DB 打开与 quick_check） |
| 打开面板首帧 | < 150 ms | 从托盘点击到首帧绘制 |
| 年查询（5 万行聚合） | < 15 ms | `bench_query` + `QElapsedTimer` |
| 磁盘增长 | < 2 MB/年 | 检查 DB 文件与 WAL 大小 |
| 进程数/句柄 | 1 进程；句柄泄漏 0（soak 前后对比） | `GetGuiResources` |

**空闲时到底有什么在跑？** 无按键 → 无消息；无脏数据 → 无定时器；托盘图标 60 s 一次（仅在数值区间变化时重绘）。这是"零轮询"设计的直接结果。

---

## 11. 可靠性、隐私、安全

- **隐私红线**：DB 中只存在 `(day, pid, press, corr)` 这类聚合计数；**不存按键序列**、不存窗口标题/进程名、不存输入文本。纠错归因用的 32 项环形缓冲仅存在于内存、进程结束即消失，且不参与任何落盘或日志输出。
- **无网络**：不链 Qt Network 的对外能力（只用 QLocalServer 做单实例），链接期即保证无法外发。
- **反误报**：不注入其他进程、不安装全局钩子、不写系统目录、不请求管理员权限 → 降低被安全软件拦截的概率。
- **权限最小化**：全程普通用户权限，仅写 HKCU 与 `%APPDATA%`。
- **可审计**：数据文件位置、schema、导出功能都开放给用户；提供"一键清空"。

---

## 12. 测试策略

| 层级 | 内容 |
|------|------|
| 单元测试（QTest，无 GUI） | ① `Periods`：跨月/跨年/闰年/ISO 周/周起始日切换/时区与时钟回拨；② `HeatRamp`：归一化边界、OKLab 插值单调性、对比度切换；③ `KeyCatalog`：pid 编解码（E0/E1/右 Shift/小键盘回车）、伪事件过滤；④ `CorrectionTracker`：连续退格、退格多于输入、导航键打断、10 s 超时；⑤ `StatsStore`：upsert 幂等性、迁移、损坏库隔离 |
| 集成测试（注入） | 用 `SendInput` 注入 F13–F24（真实键盘不会产生，避免污染真实数据与其他窗口）序列 → 断言 DB 中 `press/corr` 精确值；覆盖"长按自动重复不计数"（`SendInput` 连发 + 真实按住模拟） |
| 布局数据校验 | 遍历内置布局 JSON：pid 唯一、坐标不重叠、行宽一致、所有可计数 pid 归属于某键 |
| 性能基准 | `bench_pump` / `bench_query` 纳入 CI 手跑门槛（超出预算即失败） |
| 手工验收（每次发布） | 提权窗口、UAC、锁屏、休眠唤醒、150%/200% 缩放、多显示器（本机 3 屏）、RDP 会话、自启开关、单实例唤起、30 分钟长跑内存曲线 |

---

## 13. 构建、打包、目录

```
CMake ≥ 3.21，C++20，MSVC 2022 x64
CMAKE_AUTOMOC/AUTORCC ON，/utf-8，/W4 /permissive-，Release + LTCG
目标：keyCounter（单 exe，无额外 DLL 除 Qt 运行时）
```

`tools/deploy.ps1`：

```powershell
windeployqt --release --no-translations --no-opengl-sw --no-system-d3d-compiler `
            --no-compiler-runtime build/keyCounter.exe
# 关键：必须复制 sqldrivers/qsqlite.dll，否则 SQLite 驱动加载失败
Copy-Item D:/Qt6/6.8.2/msvc2022_64/plugins/sqldrivers/qsqlite.dll build/sqldrivers/ -Force
```

产物目录：`build/`（git 忽略）、`dist/keyCounter/`（免安装绿色目录：exe + Qt 运行时 + qsqlite + 图标 + LICENSE）。

**代码结构（实现时）**

```
src/
├─ main.cpp
├─ app/     Application / TrayController / Settings / AutoStart / SingleInstance
├─ core/    RawInputWindow / KeyEventPump / KeyCatalog / CounterEngine / CorrectionTracker
│           Periods / StatsStore(线程) / StatsQuery / RangeKey
├─ layout/  LayoutModel / LayoutRepository(JSON) / layouts/{ansi-104,tkl-87,compact-68}.json
└─ ui/      MainWindow / KeyboardHeatmapWidget / HeatRamp / TrendChartWidget /
           TopKeysWidget / SummaryCards / PeriodSelector / SettingsDialog / Theme / Icons
resources/  keycounter.qrc / icons/*.svg / app.ico / app.rc
tests/      tst_periods / tst_heatramp / tst_catalog / tst_correction / tst_store / bench_pump / bench_query
tools/      deploy.ps1 / build.ps1
```

---

## 附录 A：关键物理键 pid 对照（部分）

| 键 | makeCode | E0 | pid |
|----|----------|----|-----|
| Esc | 0x01 | – | 0x001 |
| 左 Ctrl / 左 Shift / 左 Alt | 0x1D / 0x2A / 0x38 | – | 0x01D / 0x02A / 0x038 |
| 右 Ctrl / 右 Alt | 0x1D / 0x38 | E0 | 0x11D / 0x138 |
| 右 Shift | 0x36 | – | 0x036（自身扫描码即与左 Shift 不同）|
| 主回车 / 小键盘回车 | 0x1C / 0x1C | – / E0 | 0x01C / 0x11C |
| ↑ ↓ ← → | 0x48/0x50/0x4B/0x4D | E0 | 0x148 / 0x150 / 0x14B / 0x14D |
| LWin / RWin | 0x5B / 0x5C | E0 | 0x15B / 0x15C |
| Backspace / Delete | 0x0E / 0x53 | – / E0 | 0x00E / 0x153 |
| Pause | 0x45 | E1 | 0x245 |
| PrintScreen | 0x37 | E0 | 0x137 |

（完整表在实现时由布局 JSON 与 `KeyCatalog` 单一来源生成，二者做一致性单元测试。）

## 附录 B：配置项（QSettings，HKCU\Software\keyCounter）

| 键 | 类型 | 默认 |
|----|------|------|
| `general/autostart` | bool | false |
| `general/theme` | enum | `system` |
| `general/heatRamp` | enum | `ember` |
| `general/layout` | string | 首启动引导选择，默认 `ansi-104` |
| `general/weekStart` | int | 1（周一） |
| `general/reduceMotion` | bool | false |
| `general/hotkeyEnabled` | bool | false |
| `general/hotkeySequence` | string | `Ctrl+Alt+K` |
| `stats/errorMode` | enum | `attributed`（归因口径） |
| `stats/trackHours` | bool | true |
| `stats/retentionYears` | int | 0（永久） |
| `data/backup` | bool | true |
| `ui/geometry` `ui/windowState` `ui/lastPeriod` | QByteArray/int | – |
