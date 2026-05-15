import QtQuick
import QtQuick.Window
import Qt.labs.platform as Platform

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
            text: desktopShell.alwaysOnTop ? "取消置顶" : "始终置顶"
            onTriggered: desktopShell.toggleAlwaysOnTop()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "退出"
            onTriggered: Qt.quit()
        }
    }

    // 当前阶段先直接显示一个旧版站立动画。
    // 后续 Pet Runtime 会接管动画选择，不再让 QML 写死资源路径。
    AnimatedImage {
        id: pet

        anchors.centerIn: parent
        source: "qrc:/pet/stand-right.gif"
        cache: false
        playing: true
        fillMode: Image.PreserveAspectFit
        width: 200
        height: 200
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
