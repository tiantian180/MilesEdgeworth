cmake_minimum_required(VERSION 3.21)

# 根目录只保留 v2 的聚合入口，方便 Cursor / VS Code / CMake Tools
# 直接打开仓库根目录时识别到新版桌宠，而不是旧版 Qt Widgets 工程。
project(MilesEdgeworthV2 VERSION 0.2.0 LANGUAGES CXX)

# clangd / Cursor 需要 compile_commands.json 才能知道每个 C++ 文件的
# 真实 include 路径、宏定义和 Qt 模块参数；否则编辑器会把 Qt 头文件标红。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_subdirectory(apps/desktop)

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        # 这个检查守住 macOS 桌宠窗口策略的边界：普通置顶开关必须保持可逆，
        # 不能重新依赖 SkyLight 私有 Space。
        add_test(
            NAME check_macos_window_behavior
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_macos_window_behavior.py
        )

        # Phase 0.5 的桌宠运行时检查：守住 QML singleton、PetRuntime、
        # manifest 和动画绑定的最小纵切。
        add_test(
            NAME check_phase_0_5_pet_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_5_pet_runtime.py
        )

        # Phase 0.6 的动画运行时检查：守住 manifest v2、左右朝向、
        # 一次性动作回 idle 和 QML 帧完成通知的最小骨架。
        add_test(
            NAME check_phase_0_6_animation_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_6_animation_runtime.py
        )

        # Phase 0.7 的 phase-aware 动画运行时检查：守住 enter/loop/exit
        # 这种多阶段 action 的最小结构。
        add_test(
            NAME check_phase_0_7_phase_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_7_phase_runtime.py
        )

        # Phase 0.8 的 recipe / actionPool 检查：守住“动作编排”和
        # “候选池抽取”这两个入口，后续随机 idle、菜单动作、交互事件都走这里。
        add_test(
            NAME check_phase_0_8_recipe_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_8_recipe_runtime.py
        )

        # Phase 0.9 的启动和随机 idle 检查：守住旧版“公文包入场”
        # 以及站立时随机转身 / 小动作的最小手感骨架。
        add_test(
            NAME check_phase_0_9_startup_idle_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_9_startup_idle_runtime.py
        )

        # Phase 0.10 的移动检查：守住旧版 8 方向 walk/run 资源、
        # 每帧移动增量和 QML 窗口移动入口。
        add_test(
            NAME check_phase_0_10_locomotion_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_10_locomotion_runtime.py
        )

        # Phase 0.11 的单击分区检查：守住 hitZones、click pools
        # 和 QML 左键点击入口。
        add_test(
            NAME check_phase_0_11_hit_zone_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_11_hit_zone_runtime.py
        )

        # Phase 0.12 的双击音效检查：守住 doubleClick action pool、
        # 语音资源、SoundEffect 和运行时声音播放入口。
        add_test(
            NAME check_phase_0_12_double_click_sound_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_12_double_click_sound_runtime.py
        )

        # Phase 0.13 的拖拽晃动检查：守住旧版“左右快速晃动触发蹲下，
        # 松手后站起”的最小交互链路。
        add_test(
            NAME check_phase_0_13_drag_shake_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_13_drag_shake_runtime.py
        )

        # Phase 0.14 的检察官徽章检查：守住 Take that 触发 Prop、
        # 点击徽章鞠躬、自然消失捡徽章的最小链路。
        add_test(
            NAME check_phase_0_14_prosecutor_badge_runtime
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_14_prosecutor_badge_runtime.py
        )

        # Phase 0.15 的旧版菜单设置检查：守住静音、语音语言
        # 和禁止自动移动这三个核心体验开关。
        add_test(
            NAME check_phase_0_15_menu_audio_movement_settings
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_15_menu_audio_movement_settings.py
        )

        # Phase 0.16 的旧版菜单动作检查：守住“喂食红茶”
        # 和“睡觉 / 唤醒”从测试入口进入正式右键菜单的链路。
        add_test(
            NAME check_phase_0_16_menu_tea_sleep_wake
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_16_menu_tea_sleep_wake.py
        )

        # Phase 0.17 的核心手感覆盖检查：集中守住 active goal
        # 明确列出的随机 idle、移动、点击、双击、喝茶、睡眠、晃动、徽章和音效。
        add_test(
            NAME check_phase_0_17_core_handfeel_coverage
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_17_core_handfeel_coverage.py
        )
    endif()
endif()
