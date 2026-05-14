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
        acceptedButtons: Qt.LeftButton

        property real pressX: 0
        property real pressY: 0

        onPressed: function(mouse) {
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
