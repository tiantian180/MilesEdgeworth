# Phase 0.65 架构门禁：把阶段记录里的“已知硬编码点”变成可执行检查。
#
# 当前阶段这些检查预期会失败，所以 CTest 里会暂时标记 WILL_FAIL。
# 后续 Phase 0.67-0.73 每消掉一组硬编码，就把对应断言从 WILL_FAIL 债务
# 翻成普通通过项，避免通用框架继续混入 Miles 皮肤专属逻辑。

if(NOT DEFINED PROJECT_SOURCE_DIR)
    message(FATAL_ERROR "PROJECT_SOURCE_DIR is required")
endif()

set(_HAS_FAILURE FALSE)

function(_check_absent ITEM_ID PHASE FILE_PATH TOKEN DESCRIPTION)
    set(_FULL_PATH "${PROJECT_SOURCE_DIR}/${FILE_PATH}")
    if(NOT EXISTS "${_FULL_PATH}")
        message(SEND_ERROR "[${ITEM_ID}] missing file: ${FILE_PATH}")
        set(_HAS_FAILURE TRUE PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND grep -F -n -- "${TOKEN}" "${_FULL_PATH}"
        RESULT_VARIABLE _GREP_RESULT
        OUTPUT_VARIABLE _GREP_OUTPUT
        ERROR_VARIABLE _GREP_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(_GREP_RESULT EQUAL 0)
        message(SEND_ERROR
            "[${ITEM_ID}] Phase ${PHASE}: ${DESCRIPTION}\n"
            "  Forbidden token: ${TOKEN}\n"
            "  File: ${FILE_PATH}\n"
            "  Hits:\n${_GREP_OUTPUT}"
        )
        set(_HAS_FAILURE TRUE PARENT_SCOPE)
    elseif(NOT _GREP_RESULT EQUAL 1)
        message(SEND_ERROR
            "[${ITEM_ID}] grep failed while checking ${FILE_PATH}: ${_GREP_ERROR}"
        )
        set(_HAS_FAILURE TRUE PARENT_SCOPE)
    endif()
endfunction()

# 1. 拖拽晃动分支应由 GestureTracker + behavior rules 表达。
_check_absent(1 0.67
    "apps/desktop/src/pet/PetRuntime.cpp"
    "drag_crouch"
    "drag shake crouch action must not live in PetRuntime"
)
_check_absent(1 0.67
    "apps/desktop/src/pet/PetRuntime.cpp"
    "drag_stand_up_full"
    "drag full stand-up action must not live in PetRuntime"
)
_check_absent(1 0.67
    "apps/desktop/src/pet/PetRuntime.cpp"
    "drag_stand_up_quick"
    "drag quick stand-up action must not live in PetRuntime"
)

# 2. 睡眠菜单应读取 rest capability，而不是写死 Miles recipe。
_check_absent(2 0.68
    "apps/desktop/src/pet/interaction/InteractionPipeline.cpp"
    "sleep.enterLoopExit"
    "sleep menu command must resolve through rest capability"
)

# 3. 睡眠状态应读取 rest capability，而不是比较 action id。
_check_absent(3 0.68
    "apps/desktop/src/pet/PetRuntime.h"
    "m_currentActionId == \"sleep\""
    "sleep state must not compare Miles sleep action id"
)
_check_absent(3 0.68
    "apps/desktop/src/pet/PetRuntime.cpp"
    "m_currentActionId == \"sleep\""
    "sleep state must not compare Miles sleep action id"
)

# 4. 启动入场期间是否阻止交互应来自 action 配置。
_check_absent(4 0.68
    "apps/desktop/src/pet/PetRuntime.cpp"
    "briefcase_in"
    "pointer blocking must use action blocksPointerInteraction"
)

# 5. 启动序列应由 runtime.started trigger 选择。
_check_absent(5 0.68
    "apps/desktop/src/pet/PetRuntime.cpp"
    "startup.briefcase"
    "startup recipe must be selected by runtime.started trigger"
)

# 6. walk/run 播完后的续接动作应由 action.completed trigger 选择。
_check_absent(6 0.68
    "apps/desktop/src/pet/PetRuntime.cpp"
    "walk.finished"
    "walk follow-up pool must be configured by action.completed trigger"
)
_check_absent(6 0.68
    "apps/desktop/src/pet/PetRuntime.cpp"
    "run.finished"
    "run follow-up pool must be configured by action.completed trigger"
)

# 7. movementDirection 到 facing 的映射应来自 manifest.movementFacingMap。
_check_absent(7 0.68
    "apps/desktop/src/pet/PetRuntime.cpp"
    "QStringLiteral(\"east\")"
    "movement-to-facing mapping must be manifest-driven"
)
_check_absent(7 0.68
    "apps/desktop/src/pet/PetRuntime.cpp"
    "QStringLiteral(\"west\")"
    "movement-to-facing mapping must be manifest-driven"
)

# 8. 画布与图像尺寸应来自 manifest.canvas。
_check_absent(8 0.69
    "apps/desktop/src/pet/PetRuntime.h"
    "120.0"
    "pet window size must be manifest-driven"
)
_check_absent(8 0.69
    "apps/desktop/src/pet/PetRuntime.h"
    "100.0"
    "pet image size must be manifest-driven"
)

# 9. 尺寸档位和语音语言不应硬编码在 PetRuntime。
_check_absent(9 0.69
    "apps/desktop/src/pet/PetRuntime.cpp"
    "\"mini\""
    "pet sizes must be manifest-driven"
)
_check_absent(9 0.69
    "apps/desktop/src/pet/PetRuntime.cpp"
    "\"small\""
    "pet sizes must be manifest-driven"
)
_check_absent(9 0.69
    "apps/desktop/src/pet/PetRuntime.cpp"
    "\"medium\""
    "pet sizes must be manifest-driven"
)
_check_absent(9 0.69
    "apps/desktop/src/pet/PetRuntime.cpp"
    "\"big\""
    "pet sizes must be manifest-driven"
)
_check_absent(9 0.72
    "apps/desktop/src/pet/PetRuntime.cpp"
    "\"jp\""
    "voice language must move to optional Audio Capability"
)
_check_absent(9 0.72
    "apps/desktop/src/pet/PetRuntime.cpp"
    "\"en\""
    "voice language must move to optional Audio Capability"
)
_check_absent(9 0.72
    "apps/desktop/src/pet/PetRuntime.cpp"
    "\"zh\""
    "voice language must move to optional Audio Capability"
)

# 10. 表面层判断 idle loop 不应写死 Miles idle action。
_check_absent(10 0.69
    "apps/desktop/src/pet/surface/PetSurfaceWindow.cpp"
    "idle_stand"
    "surface idle loop detection must be manifest-driven"
)

# 11. CustomInteractionRegistry 不应停留在空壳。
_check_absent(11 0.70
    "apps/desktop/src/pet/interaction/CustomInteractionRegistry.cpp"
    "Q_UNUSED(event)"
    "custom interaction registry must dispatch real handlers"
)

if(_HAS_FAILURE)
    message(FATAL_ERROR "Phase 0.65 architecture guardrails found known framework boundary debt")
endif()

message(STATUS "Phase 0.65 architecture guardrails passed")
