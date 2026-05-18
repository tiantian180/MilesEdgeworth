import QtQuick
import QtQuick.Window
import Qt.labs.platform as Platform
import MilesEdgeworth as App

Window {
    id: petWindow

    width: 240
    height: 240
    visible: true
    color: "transparent"
    title: "MilesEdgeworth v2"

    flags: Qt.FramelessWindowHint
           | Qt.NoDropShadowWindowHint

    // Phase 0 先使用平台原生菜单承载最小操作入口。
    // 这样菜单的 hover、外部点击关闭、阴影和系统质感都交给 Qt/系统处理。
    Platform.Menu {
        id: contextMenu

        Platform.MenuItem {
            text: App.DesktopShell.alwaysOnTop ? "取消置顶" : "始终置顶"
            onTriggered: App.DesktopShell.toggleAlwaysOnTop()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "回到待机"
            onTriggered: App.PetRuntime.returnToIdle()
        }

        Platform.MenuItem {
            text: App.PetRuntime.currentFacing === "right" ? "切到朝左" : "切到朝右"
            onTriggered: App.PetRuntime.toggleFacing()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "测试思考"
            onTriggered: App.PetRuntime.testThinking()
        }

        Platform.MenuItem {
            text: "测试说话"
            onTriggered: App.PetRuntime.testSpeaking()
        }

        Platform.MenuItem {
            text: "测试异议"
            onTriggered: App.PetRuntime.testObjecting()
        }

        Platform.MenuItem {
            text: "测试鞠躬"
            onTriggered: App.PetRuntime.testBow()
        }

        Platform.MenuItem {
            text: "测试喝茶"
            onTriggered: App.PetRuntime.testTea()
        }

        Platform.MenuItem {
            text: "测试睡觉"
            onTriggered: App.PetRuntime.testSleep()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "退出"
            onTriggered: Qt.quit()
        }
    }

    // Phase 0.8 先用一个轻量 Timer 模拟旧版“待机时偶尔做点小动作”。
    // 真正能不能触发由 PetRuntime 决定，避免 Timer 打断正在播放的交互动作。
    Timer {
        id: idleRandomTimer

        interval: 7000
        repeat: true
        running: petWindow.visible
        onTriggered: App.PetRuntime.triggerIdle()
    }

    // QML 只负责播放当前动画，具体 state/action 到资源的选择交给 PetRuntime。
    // 这样将来接入模型事件、点击交互、移动状态时，不需要反复改表现层。
    AnimatedImage {
        id: pet

        anchors.centerIn: parent
        source: App.PetRuntime.currentAnimationUrl
        cache: false
        playing: true
        fillMode: Image.PreserveAspectFit
        width: 200
        height: 200

        onCurrentFrameChanged: {
            if ((App.PetRuntime.currentAutoReturnToIdle
                    || App.PetRuntime.currentLoopMode === "once")
                    && frameCount > 0
                    && currentFrame >= frameCount - 1) {
                App.PetRuntime.handleAnimationFinished()
            }
        }
    }

    Connections {
        target: App.PetRuntime

        function onPlaybackSerialChanged() {
            // 同一个 action 连续触发时，source URL 可能不变。
            // playbackSerial 变化代表“这次要重新播放”，所以这里手动回到第 0 帧。
            pet.currentFrame = 0
            pet.playing = false
            pet.playing = true
        }
    }

    // Phase 0 先用最容易读懂的拖拽逻辑。
    // 后续如果要做到像旧版一样的像素级点击区域，需要交给 Pet Runtime 和 hit mask。
    MouseArea {
        id: dragArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 1

        property real pressX: 0
        property real pressY: 0

        onPressed: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                contextMenu.open()
                return
            }

            pressX = mouse.x
            pressY = mouse.y
        }

        onPositionChanged: function(mouse) {
            if ((mouse.buttons & Qt.LeftButton) === 0) {
                return
            }

            petWindow.x += mouse.x - pressX
            petWindow.y += mouse.y - pressY
        }
    }
}
