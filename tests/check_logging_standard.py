#!/usr/bin/env python3
"""Check the unified logging-standard implementation."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    file_path = ROOT / path
    if not file_path.exists():
        raise AssertionError(f"missing file: {path}")
    return file_path.read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    mileslog = read("apps/agent-core/internal/mileslog/mileslog.go")
    mileslog_test = read("apps/agent-core/internal/mileslog/mileslog_test.go")
    main_go = read("apps/agent-core/cmd/miles-agent/main.go")
    provider_go = read("apps/agent-core/internal/chat/openai/provider.go")
    qt_handler_h = read("apps/desktop/src/logging/MilesLogHandler.h")
    qt_handler_cpp = read("apps/desktop/src/logging/MilesLogHandler.cpp")
    qt_handler_smoke = read("apps/desktop/tests/logging_formatter_smoke.cpp")
    desktop_main = read("apps/desktop/src/main.cpp")
    chat_cpp = read("apps/desktop/src/chat/ChatController.cpp")
    desktop_cmake = read("apps/desktop/CMakeLists.txt")
    root_cmake = read("CMakeLists.txt")

    require("package mileslog" in mileslog, "Go mileslog package must exist")
    require("func New(category string) *slog.Logger" in mileslog,
            "mileslog.New(category) must be exported")
    require("func PayloadLoggingEnabled() bool" in mileslog,
            "mileslog must expose PayloadLoggingEnabled")
    require("type handler struct" in mileslog and "slog.Handler" in mileslog,
            "mileslog must implement a custom slog.Handler")
    require("runtime.CallersFrames" in mileslog and "record.PC" in mileslog,
            "mileslog must extract source from record.PC")
    require("MILES_LOG_LEVEL" in mileslog and "MILES_LOG_FILE" in mileslog
            and "MILES_LOG_PAYLOADS" in mileslog,
            "mileslog must read all logging env vars")
    require("TextHandler" not in mileslog,
            "mileslog must not use slog.TextHandler")
    require(".WithGroup(" not in mileslog and "slog.WithGroup" not in mileslog,
            "mileslog must not use slog.WithGroup for category")
    require("category" in mileslog and "MILES." in mileslog_test,
            "mileslog tests must cover category formatting")

    require("MILES_DEBUG_CHAT" not in main_go + provider_go,
            "MILES_DEBUG_CHAT must be removed")
    require("SetDebugLogging" not in provider_go,
            "openai.SetDebugLogging must be removed")
    require("debugf(" not in provider_go,
            "provider debugf helper must be removed")
    require("log.Printf" not in main_go + provider_go,
            "sidecar must not use log.Printf")
    require("log.Fatalf" not in main_go + provider_go,
            "sidecar must not use log.Fatalf")
    require("mileslog.New(\"MILES.SIDECAR\")" in main_go,
            "main.go must use MILES.SIDECAR logger")
    require("mileslog.New(\"MILES.CHAT.PROVIDER\")" in provider_go,
            "provider.go must use MILES.CHAT.PROVIDER logger")

    require("namespace MilesLogHandler" in qt_handler_h,
            "Qt logging handler namespace must be declared")
    require("void install()" in qt_handler_h,
            "Qt logging handler must expose install()")
    require("qInstallMessageHandler" in qt_handler_cpp,
            "Qt logging handler must install a message handler")
    require("qFormatLogMessage" in qt_handler_cpp,
            "Qt logging handler must preserve default formatting for non-miles categories")
    require("MILES_LOG_FILE" in qt_handler_cpp,
            "Qt logging handler must support MILES_LOG_FILE")
    require("miles.*" in qt_handler_cpp or "miles." in qt_handler_cpp,
            "Qt logging handler must special-case miles.* categories")
    require("MilesLogHandler::install()" in desktop_main,
            "main.cpp must install the Qt logging handler")
    require("LoggingFormatterSmoke" in desktop_cmake and "logging_formatter_smoke" in desktop_cmake,
            "Qt logging formatter smoke test must be registered")
    require("check_logging_standard" in root_cmake,
            "root CMake must register check_logging_standard")

    require("ForwardedErrorChannel" in chat_cpp,
            "ChatController must forward sidecar stderr")
    require("readyReadStandardError" not in chat_cpp,
            "ChatController must not re-log sidecar stderr")
    require("logProcessOutput" not in chat_cpp,
            "ChatController logProcessOutput helper must be removed")
    require("MILES_LOG_PAYLOADS" in chat_cpp,
            "ChatController must forward MILES_LOG_PAYLOADS to sidecar")

    print("logging standard contract ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
