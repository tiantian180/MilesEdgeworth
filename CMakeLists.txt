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

        # Phase 0.19 的窗口移动边界检查：守住旧版“走路/跑步不会把
        # 桌宠带出屏幕”的基础手感。
        add_test(
            NAME check_phase_0_19_window_movement_bounds
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_19_window_movement_bounds.py
        )

        # Phase 0.20 的移动续行检查：守住旧版 walk/run 播完后
        # 可能继续移动、换方向或停下的生命感。
        add_test(
            NAME check_phase_0_20_locomotion_followup
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_20_locomotion_followup.py
        )

        # Phase 0.21 的单击分区检查：守住旧版“不同朝向下点击
        # 脸、手臂、腿等部位仍能触发对应反应”的手感。
        add_test(
            NAME check_phase_0_21_facing_hit_zones
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_21_facing_hit_zones.py
        )

        # Phase 0.22 的 polygon hit zone 检查：把旧版点击脸部使用的
        # 斜线区域表达进 manifest，避免只能用粗矩形近似。
        add_test(
            NAME check_phase_0_22_polygon_hit_zones
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_22_polygon_hit_zones.py
        )

        # Phase 0.23 的启动入场交互保护检查：守住旧版公文包入场时
        # 不响应点击、双击和拖拽的节奏。
        add_test(
            NAME check_phase_0_23_startup_interaction_guard
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_23_startup_interaction_guard.py
        )

        # Phase 0.24 的双击语言候选池检查：守住旧版中文语音下
        # 不进入 Eureka 分支的细节。
        add_test(
            NAME check_phase_0_24_language_double_click_pool
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_24_language_double_click_pool.py
        )

        # Phase 0.25 的旧版随机待机动作检查：确保 once/0..19
        # 里已经确认语义的动作对都进入 v2 idle.random。
        add_test(
            NAME check_phase_0_25_legacy_idle_once_pool
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_25_legacy_idle_once_pool.py
        )

        # Phase 0.26 的随机待机补充检查：抬头和低头动作既可由
        # 单击触发，也要保留旧版 once 随机待机的使用场景。
        add_test(
            NAME check_phase_0_26_idle_look_variants
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_26_idle_look_variants.py
        )

        # Phase 0.27 的站立朝向检查：旧版随机待机中存在一条
        # 直接切到反向站立的分支，不播放转身动画。
        add_test(
            NAME check_phase_0_27_idle_flip_stand
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_27_idle_flip_stand.py
        )

        # Phase 0.28 的菜单喝茶检查：旧版 tea*.gif 本身已经包含
        # 喝茶后的致意动作，v2 不再额外串联 bow.gif。
        add_test(
            NAME check_phase_0_28_legacy_tea_once
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_28_legacy_tea_once.py
        )

        # Phase 0.29 的拖拽晃动 smoke 检查：确保 PetRuntime
        # 真实模拟过“左右晃到蹲下、松手快站/完整站起”两条旧版分支。
        add_test(
            NAME check_phase_0_29_drag_shake_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_29_drag_shake_smoke.py
        )

        # Phase 0.30 的检察官徽章 smoke 检查：确保 PetRuntime
        # 真实等待 Prop 生成，并覆盖点击鞠躬和自然消失捡徽章两条分支。
        add_test(
            NAME check_phase_0_30_prosecutor_badge_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_30_prosecutor_badge_smoke.py
        )

        # Phase 0.31 的睡眠鼠标交互 smoke 检查：确保睡眠循环中
        # 单击不打断睡眠，双击能进入 wake/exit 唤醒分支。
        add_test(
            NAME check_phase_0_31_sleep_pointer_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_31_sleep_pointer_smoke.py
        )

        # Phase 0.32 的单击分区 smoke 检查：确保头部、大臂、
        # 小臂、胸口、肚子和腿部都经过真实 PetRuntime 点击路径。
        add_test(
            NAME check_phase_0_32_single_click_zone_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_32_single_click_zone_smoke.py
        )

        # Phase 0.33 的双击 recipe smoke 检查：确保四个旧版
        # 双击语音动作都能由 PetRuntime 播放动作和默认音效。
        add_test(
            NAME check_phase_0_33_double_click_recipe_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_33_double_click_recipe_smoke.py
        )

        # Phase 0.34 的 8 方向移动 smoke 检查：确保 walk/run
        # 全方向都经过 PetRuntime，且 run 的移动速度大于 walk。
        add_test(
            NAME check_phase_0_34_locomotion_direction_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_34_locomotion_direction_smoke.py
        )

        # Phase 0.35 的随机 idle recipe smoke 检查：确保非移动
        # idle 候选都能从 recipe 走到预期 action。
        add_test(
            NAME check_phase_0_35_idle_random_recipe_smoke
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_35_idle_random_recipe_smoke.py
        )

        # Phase 0.36 的随机 idle 权重检查：确保 idle.random
        # 贴近旧版 STAND 自动切换的 8/12/60/10/10 概率结构。
        add_test(
            NAME check_phase_0_36_idle_random_legacy_weights
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_36_idle_random_legacy_weights.py
        )

        # Phase 0.37 的随机 idle 触发检查：确保随机待机
        # 由站立循环完成事件驱动，而不是固定间隔 Timer。
        add_test(
            NAME check_phase_0_37_idle_loop_trigger
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_37_idle_loop_trigger.py
        )

        # Phase 0.38 的移动续行权重检查：确保 walk/run 播完后
        # 的自由游走概率贴近旧版每轮动画结束时的分布。
        add_test(
            NAME check_phase_0_38_locomotion_followup_legacy_weights
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_38_locomotion_followup_legacy_weights.py
        )

        # Phase 0.39 的双击判定窗口检查：确保单击延迟确认
        # 使用旧版 300ms，避免双击体感偏紧。
        add_test(
            NAME check_phase_0_39_double_click_window
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_39_double_click_window.py
        )

        # Phase 0.40 的喝茶入口检查：确保红茶从皮肤命令进入，
        # 复用正式菜单喝茶逻辑，不再引用早期废弃 recipe。
        add_test(
            NAME check_phase_0_40_test_tea_entry
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_40_test_tea_entry.py
        )

        # Phase 0.41 的单击子区域检查：确保肚子和腿部
        # 按旧版坐标分支拆成确定性区域，而不是粗区域随机池。
        add_test(
            NAME check_phase_0_41_split_click_subzones
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_41_split_click_subzones.py
        )

        # Phase 0.42 的旧版尺寸菜单检查：确保迷你/小/中/大
        # 四档 scale 同时影响窗口尺寸、动画尺寸和移动步长。
        add_test(
            NAME check_phase_0_42_legacy_size_menu
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_42_legacy_size_menu.py
        )

        # Phase 0.43 的徽章缩放检查：旧版调整大小时，检察官徽章的
        # 起点、飞行距离和视觉尺寸也会跟随 scale。
        add_test(
            NAME check_phase_0_43_scaled_prosecutor_badge
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_43_scaled_prosecutor_badge.py
        )

        # Phase 0.44 的系统托盘检查：还原旧版托盘常驻、
        # 托盘右键退出和单击唤起桌宠窗口。
        add_test(
            NAME check_phase_0_44_system_tray
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_44_system_tray.py
        )

        # Phase 0.45 的旧版启动定位检查：旧版会从屏幕左下角
        # 略微越界的位置播放公文包入场，v2 需要保留这个第一印象。
        add_test(
            NAME check_phase_0_45_legacy_startup_position
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_45_legacy_startup_position.py
        )

        # Phase 0.46 的旧版身体边界检查：旧版不是按透明窗口外壳夹取，
        # 而是按角色身体附近的点位限制活动范围。
        add_test(
            NAME check_phase_0_46_legacy_body_bounds
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_46_legacy_body_bounds.py
        )

        # Phase 0.47 的旧版双屏选项检查：恢复“单屏 / 主屏幕在左侧 /
        # 主屏幕在右侧”菜单入口，并让双屏模式使用虚拟桌面横向边界。
        add_test(
            NAME check_phase_0_47_legacy_dual_screen_options
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_47_legacy_dual_screen_options.py
        )

        # Phase 0.48 的旧版手型光标检查：旧版 enterEvent 会把鼠标
        # 设成 PointingHandCursor，提示桌宠可以点击和拖拽。
        add_test(
            NAME check_phase_0_48_legacy_hand_cursor
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_48_legacy_hand_cursor.py
        )

        # Phase 0.49 的旧版双击释放检查：旧版第二次点击会取消单击计时器，
        # 松手后只触发双击动作，不能再补排一次单击分区反应。
        add_test(
            NAME check_phase_0_49_double_click_release_guard
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_49_double_click_release_guard.py
        )

        # Phase 0.50 的旧版静音音量检查：旧版会直接 setVolume(0)，
        # v2 的 QML SoundEffect 也要跟随 audioMuted 立即变为 0。
        add_test(
            NAME check_phase_0_50_mute_volume_binding
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_50_mute_volume_binding.py
        )

        # Phase 0.52 的 idle loop 触发配置检查：站立循环播完后
        # 是否进入随机待机池，应由皮肤 manifest 决定，避免运行时硬编码概率。
        add_test(
            NAME check_phase_0_52_manifest_idle_loop_trigger
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_52_manifest_idle_loop_trigger.py
        )

        # Phase 0.54 的皮肤资源布局检查：GIF、音频和 Prop 图片应迁入
        # skin assets 目录，但 qrc:/pet 和 qrc:/audio 对外 alias 保持稳定。
        add_test(
            NAME check_phase_0_54_assets_layout
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_54_assets_layout.py
        )

        # Phase 0.55 的 manifest 结构拆分检查：PetRuntime 不再内联维护
        # manifest schema 数据结构，而是通过 SkinManifest 承载皮肤配置。
        add_test(
            NAME check_phase_0_55_pet_runtime_manifest_split
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_55_pet_runtime_manifest_split.py
        )

        # Phase 0.56 的运行时服务拆分检查：稳定的 manifest 加载逻辑
        # 应离开 PetRuntime，后续 selector / trigger engine 也走同一方向。
        add_test(
            NAME check_phase_0_56_pet_runtime_service_split
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_56_pet_runtime_service_split.py
        )

        # Phase 0.57 的 hit zone matcher 拆分检查：单击区域的坐标归一化、
        # 朝向 rect / polygon 选择和命中判断应离开 PetRuntime。
        add_test(
            NAME check_phase_0_57_hit_zone_matcher
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_57_hit_zone_matcher.py
        )

        # Phase 0.58 的菜单和定制交互边界检查：开发测试入口应移除，
        # 红茶应通过皮肤定制命令进入，而不是暴露为通用 Runtime API。
        add_test(
            NAME check_phase_0_58_menu_custom_boundary
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_58_menu_custom_boundary.py
        )

        # Phase 0.59 的事件到 ActionRequest 架构检查：QML 只提交事件，
        # 交互层把事件转换为请求，PetRuntime 只执行最终 ActionRequest。
        add_test(
            NAME check_phase_0_59_event_action_request_spine
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_phase_0_59_event_action_request_spine.py
        )
    endif()
endif()
