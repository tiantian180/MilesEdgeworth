cmake_minimum_required(VERSION 3.21)

# 根目录只保留 v2 的聚合入口，方便 Cursor / VS Code / CMake Tools
# 直接打开仓库根目录时识别到新版桌宠，而不是旧版 Qt Widgets 工程。
project(MilesEdgeworthV2 VERSION 0.2.0 LANGUAGES CXX)

# clangd / Cursor 需要 compile_commands.json 才能知道每个 C++ 文件的
# 真实 include 路径、宏定义和 Qt 模块参数；否则编辑器会把 Qt 头文件标红。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(apps/desktop)
