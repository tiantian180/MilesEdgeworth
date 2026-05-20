# Phase 0.65 架构门禁：把阶段记录里的“已知硬编码点”变成可执行检查。
#
# 随着 Phase 0.66-0.70 逐步收口，当前已知硬编码点已经转入正常测试。
# 后续如果发现新的框架边界债务，再在这里追加明确断言。

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

if(_HAS_FAILURE)
    message(FATAL_ERROR "Phase 0.65 architecture guardrails found known framework boundary debt")
endif()

message(STATUS "Phase 0.65 architecture guardrails passed")
