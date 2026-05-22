cmake_minimum_required(VERSION 3.21)

project(MilesAgentCore LANGUAGES NONE)

find_program(GO_EXECUTABLE go)
if(NOT GO_EXECUTABLE)
    message(FATAL_ERROR "Go toolchain is required to build miles-agent")
endif()

set(MILES_AGENT_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/miles-agent${CMAKE_EXECUTABLE_SUFFIX}"
)
set(MILES_AGENT_BINARY "${MILES_AGENT_OUTPUT}" CACHE INTERNAL "Path to the miles-agent sidecar binary")

file(GLOB_RECURSE MILES_AGENT_GO_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.go"
)

add_custom_command(
    OUTPUT "${MILES_AGENT_OUTPUT}"
    COMMAND "${GO_EXECUTABLE}" build -o "${MILES_AGENT_OUTPUT}" ./cmd/miles-agent
    DEPENDS
        ${MILES_AGENT_GO_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/go.mod"
        "${CMAKE_CURRENT_SOURCE_DIR}/go.sum"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building miles-agent sidecar"
    VERBATIM
)

add_custom_target(MilesAgentCore DEPENDS "${MILES_AGENT_OUTPUT}")
