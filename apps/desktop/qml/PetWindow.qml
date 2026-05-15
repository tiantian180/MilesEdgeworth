import QtQuick
import QtQuick.Window

Window {
    id: petWindow

    width: 240
    height: 240
    visible: true
    color: "transparent"
    title: "MilesEdgeworth v2"

    flags: Qt.FramelessWindowHint
           | Qt.WindowStaysOnTopHint
           | Qt.Tool
           | Qt.NoDropShadowWindowHint

    // Phase 0 还没有系统托盘和完整右键菜单。
    // 先在桌宠窗口内部做一个极简右键浮层，避免额外创建 native popup 窗口。
    Rectangle {
        id: contextMenu

        visible: false
        z: 2
        width: 84
        height: 36
        radius: 6
        color: "#242424"
        border.color: "#6a6a6a"
        border.width: 1

        Text {
            anchors.centerIn: parent
            color: "white"
            text: "退出"
            font.pixelSize: 14
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton

            onClicked: Qt.quit()
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
                contextMenu.x = Math.min(mouse.x, petWindow.width - contextMenu.width)
                contextMenu.y = Math.min(mouse.y, petWindow.height - contextMenu.height)
                contextMenu.visible = true
                return
            }

            contextMenu.visible = false
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
