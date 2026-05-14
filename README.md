使用 Qt 6.5.1 + Visual Studio 2022 开发。现在也提供了 CMake 构建入口，方便在 macOS / Linux 上尝试编译旧版 Qt Widgets 项目。

## macOS 构建

当前 macOS 构建已在 Apple Silicon Mac + Homebrew Qt 6.11.0 上验证通过。旧项目仍是 Qt Widgets，不是 QML。

macOS 版桌宠窗口会使用 AppKit + SkyLight 私有 API 做浮层处理：窗口会进入一个系统级 stationary Space，尽量保证切换桌面空间和全屏应用时仍停留在屏幕上。这个行为参考了 [clawd-on-desk](https://github.com/rullerzhou-afk/clawd-on-desk) 的 macOS 浮层实现思路。注意：SkyLight 属于 macOS 私有接口，适合个人分发/技术验证，不适合 Mac App Store，也可能在未来 macOS 版本变化时需要维护。

安装基础工具：

```bash
xcode-select --install
brew install cmake ninja qt
```

如果 `qt-cmake`、`qmake` 不在 `PATH`，可以先临时使用 Homebrew Qt 路径：

```bash
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt"
export PATH="/opt/homebrew/opt/qt/bin:$PATH"
```

Intel Mac 的 Homebrew 默认路径通常是 `/usr/local`，对应改成：

```bash
export CMAKE_PREFIX_PATH="/usr/local/opt/qt"
export PATH="/usr/local/opt/qt/bin:$PATH"
```

配置和构建：

```bash
cmake -S . -B ../MilesEdgeworth-release-build -G Ninja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_BUILD_TYPE=Release
cmake --build ../MilesEdgeworth-release-build
open ../MilesEdgeworth-release-build/MilesEdgeworth.app
```

本机可分发 app 部署：

```bash
./scripts/package-macos.sh
```

打包脚本会清理旧的 `../MilesEdgeworth-release-build`，重新构建、运行 `macdeployqt`、裁剪旧版 Widgets 项目用不到的插件、做本地 ad-hoc 签名，并生成 `MilesEdgeworth-macOS-arm64.zip`。不要在已经 `macdeployqt` 过的 `.app` 上继续增量编译，否则可能把 Homebrew Qt 和 app 内置 Qt 同时加载进进程，导致启动崩溃。

`-no-codesign` 后需要再做一次本地 ad-hoc 签名，否则部署后的 app 可能直接退出。正式发布仍需要开发者证书、公证和 DMG 流程。

目前 macOS 版属于“能构建、能启动、能显示桌宠”的技术验证版本；托盘、置顶、多屏、音频和拖拽等交互还需要继续实测。当前部署后的 `.app` 大约 100MB。

图片素材和音频素材均来自游戏《逆转裁判》和《逆转检事》.

b站有演示：https://www.bilibili.com/video/BV1Rz42187m2/

这是我第一次从零开始一个项目一直到发布，还有很多不完善的地方，请大家多多包涵。在这里记录一下实现的内容和一点心得:

0. 窗口（桌宠本体）无边框透明置顶，不在任务栏显示（Qt::Tool），在状态栏显示(QSystemTrayIcon)，拖拽移动窗口.
    注意窗口类型设置为Qt::Tool之后关闭窗口不会退出程序，我的解决方法是重写`closeEvent()`关闭时发送信号给QApplication，让它执行`quit()`
   
1. 按照一定规律随机播放gif
   
2. 单击不同区域触发不同动画
   
3. 双击触发动画并播放语音（此处涉及到单击、双击和拖拽的区分，想了挺久的）
   
   3.1 “看招”时飞出检察官徽章涉及到多窗口问题，窗口的自动移动使用了QPropertyAnimation
   
   3.2 播放语音使用QSoundEffect，需要Qt的multimedia模块
   
4. 右键菜单的若干功能
   
5. 图片置顶查看器：以QGraphicsView为基础。支持多开，上限10个。此处涉及多窗口问题
   
   5.1 采用了双向链表结构来管理打开的若干图片查看器窗口，打开时new，关闭时delete。退出桌宠时全部delete
   
   5.2 在调试过程中发现QPixmap随着图片切换会占越来越多内存，后来查资料发现是因为加载图片时图片数据加入到QPixmapCache缓冲区上，所以通过及时调用`QPixmapCache::clear()`，就能解决内存占用过大的问题

6. 支持双屏模式下的拖拽和跨屏走动. 右键设置双屏选项, 选择"单屏"则限制走动范围在当前所在窗口, 选择"主屏幕在左侧"则在跑步时可跨越主屏幕右边界穿越到第二屏幕的左边界, 选择"主屏幕在右侧"则在跑步时可跨越主屏幕左边界穿越到第二屏幕的右边界. 选项不影响拖拽, 拖拽始终跟随鼠标.

7. 快速晃动触发害怕地震的小动画。主要也是靠的重写press,move和release的鼠标事件来实现，还搭配了一个计时器。
