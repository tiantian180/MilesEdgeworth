cmake_minimum_required(VERSION 3.21)

# 根目录只保留 v2 的聚合入口，方便 Cursor / VS Code / CMake Tools
# 直接打开仓库根目录时识别到新版桌宠，而不是旧版 Qt Widgets 工程。
project(MilesEdgeworthV2 VERSION 0.2.0 LANGUAGES CXX)

add_subdirectory(apps/desktop)
