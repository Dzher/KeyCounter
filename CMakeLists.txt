cmake_minimum_required(VERSION 3.21)

project(keyCounter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/utf-8 /W4 /permissive- /MP /Zc:__cplusplus)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    add_link_options(/OPT:REF /OPT:ICF)
endif ()

option(KC_BUILD_APP "构建 GUI 主程序" ON)
option(KC_BUILD_TESTS "构建单测与基准" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql Network Test)

# ---------------------------------------------------------------------------
# 核心库（无 UI）：采集 / 统计 / 存储 / 布局
# ---------------------------------------------------------------------------
set(KC_CORE_SOURCES
    src/core/KeyTypes.h
    src/core/KeyCatalog.h
    src/core/KeyCatalog.cpp
    src/core/CorrectionTracker.h
    src/core/CorrectionTracker.cpp
    src/core/Periods.h
    src/core/Periods.cpp
    src/core/CounterEngine.h
    src/core/CounterEngine.cpp
    src/core/StatsStore.h
    src/core/StatsStore.cpp
    src/core/StatsQuery.h
    src/core/StatsQuery.cpp
    src/core/RawInputMonitor.h
    src/core/RawInputMonitor.cpp
    src/layout/LayoutModel.h
    src/layout/LayoutModel.cpp
)

add_library(kccore STATIC ${KC_CORE_SOURCES})
target_include_directories(kccore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kccore PUBLIC Qt6::Core Qt6::Gui Qt6::Sql)
if (WIN32)
    target_link_libraries(kccore PUBLIC user32 wtsapi32)
endif ()

# ---------------------------------------------------------------------------
# 测试
# ---------------------------------------------------------------------------
if (KC_BUILD_TESTS)
    enable_testing()

    # 每个测试类一个可执行文件：QTest 的标准做法，输出不会被后一个类覆盖
    foreach (kc_test IN ITEMS tst_keycatalog tst_correction tst_periods tst_layouts tst_engine tst_store tst_rawinput)
        add_executable(${kc_test} tests/${kc_test}.cpp resources/keycounter.qrc)
        target_link_libraries(${kc_test} PRIVATE kccore Qt6::Test)
        add_test(NAME ${kc_test} COMMAND ${kc_test})
    endforeach ()

    # -----------------------------------------------------------------------
    # 热路径微基准（M1 性能验收）
    # -----------------------------------------------------------------------
    add_executable(keyCounterBench tests/bench_pump.cpp)
    target_link_libraries(keyCounterBench PRIVATE kccore)

    # -----------------------------------------------------------------------
    # 采集探针（诊断 Raw Input 是否收到按键，开发用）
    # -----------------------------------------------------------------------
    add_executable(keyCounterRawProbe tools/rawinput_probe.cpp)
    target_link_libraries(keyCounterRawProbe PRIVATE kccore)
endif ()

# ---------------------------------------------------------------------------
# 主程序（M2 起接入 UI）
# ---------------------------------------------------------------------------
if (KC_BUILD_APP)
    set(KC_APP_SOURCES
    src/main.cpp
    src/app/Application.h
    src/app/Application.cpp
    src/app/Settings.h
    src/app/Settings.cpp
    src/app/AutoStart.h
    src/app/AutoStart.cpp
    src/app/SingleInstance.h
    src/app/SingleInstance.cpp
    src/app/TrayController.h
    src/app/TrayController.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/HeatRamp.h
    src/ui/HeatRamp.cpp
    src/ui/KeyboardHeatmapWidget.h
    src/ui/KeyboardHeatmapWidget.cpp
    src/ui/SummaryCardsWidget.h
    src/ui/SummaryCardsWidget.cpp
    src/ui/TopKeysWidget.h
    src/ui/TopKeysWidget.cpp
    src/ui/TrendChartWidget.h
    src/ui/TrendChartWidget.cpp
    src/ui/PeriodSelector.h
    src/ui/PeriodSelector.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/app/Icons.h
    src/app/Icons.cpp
    resources/keycounter.qrc
)

    add_executable(keyCounter WIN32 ${KC_APP_SOURCES})
    target_link_libraries(keyCounter PRIVATE kccore Qt6::Widgets Qt6::Network)
    set_target_properties(keyCounter PROPERTIES WIN32_EXECUTABLE ON)
endif ()
