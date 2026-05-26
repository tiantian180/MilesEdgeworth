import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    property url iconSource
    property string tooltipText: ""
    property int iconSize: 20
    property color hoverFill: "#eee7da"
    property color pressedFill: "#e2d8cc"
    property bool showHoverFill: true

    width: 34
    height: 34
    implicitWidth: 34
    implicitHeight: 34
    padding: 0

    ToolTip.visible: hovered && tooltipText.length > 0
    ToolTip.text: tooltipText

    contentItem: Image {
        source: control.iconSource
        sourceSize.width: control.iconSize
        sourceSize.height: control.iconSize
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        opacity: control.enabled ? 1.0 : 0.42
        smooth: true
    }

    background: Rectangle {
        radius: 10
        color: !control.showHoverFill
                ? "transparent"
                : (control.down ? control.pressedFill : (control.hovered ? control.hoverFill : "transparent"))
    }
}
