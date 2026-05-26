import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MilesEdgeworth as App

Item {
    id: root

    property bool compactMode: false
    property url expandIconSource: "qrc:/ui-icons/maximize-2.svg"
    property url closeIconSource: "qrc:/ui-icons/x.svg"
    property url sendIconSource: canSubmit && !App.ChatController.sending
            ? "qrc:/ui-icons/arrow-up-white.svg"
            : "qrc:/ui-icons/arrow-up-muted.svg"
    property url stopIconSource: "qrc:/ui-icons/square-stop.svg"
    property alias text: input.text
    readonly property bool disconnectedInput: !App.ChatController.sidecarReady && !App.ChatController.sending
    readonly property bool providerConfigured: App.ChatController.providerConfigured
    readonly property bool canType: App.ChatController.sidecarReady && providerConfigured && !App.ChatController.sending
    readonly property bool missingProviderConfig: !providerConfigured && !App.ChatController.sending
    readonly property bool disabledInput: disconnectedInput || missingProviderConfig
    readonly property bool hasText: input.text.trim().length > 0
    readonly property bool canSubmit: App.ChatController.sending
            || (App.ChatController.sidecarReady && providerConfigured && hasText)
    readonly property bool readyToSend: canSubmit && !App.ChatController.sending
    readonly property int compactBarWidth: 380
    readonly property int inputMinHeight: compactMode ? 42 : 50
    readonly property int inputMaxHeight: compactMode ? 140 : 156
    readonly property int inputTargetHeight: Math.max(inputMinHeight,
            Math.min(inputMaxHeight, Math.ceil(input.contentHeight + 18)))
    readonly property int outerVerticalPadding: compactMode ? 7 : 14
    readonly property int buttonSize: compactMode ? 30 : 32
    readonly property int sideButtonWidth: compactMode ? 36 : 0
    readonly property int sideIconSize: 14
    readonly property int sendIconSize: 17
    readonly property int stopIconSize: 16

    implicitWidth: compactMode ? compactBarWidth : 520
    implicitHeight: inputTargetHeight + outerVerticalPadding * 2

    signal submitRequested(string text)
    signal cancelRequested()
    signal expandRequested()
    signal closeRequested()

    function clearText() {
        input.text = ""
    }

    function forceInputFocus() {
        input.forceActiveFocus()
    }

    function submitOrCancel() {
        if (!root.canSubmit) {
            return
        }

        if (App.ChatController.sending) {
            root.cancelRequested()
            return
        }

        const trimmed = input.text.trim()
        if (trimmed.length === 0) {
            return
        }
        root.submitRequested(trimmed)
    }

    RowLayout {
        anchors.fill: parent
        spacing: root.compactMode ? 6 : 8

        MilesIconButton {
            id: expandButton

            visible: root.compactMode
            iconSource: root.expandIconSource
            iconSize: root.sideIconSize
            tooltipText: ""
            showHoverFill: true
            Layout.preferredWidth: visible ? root.sideButtonWidth : 0
            Layout.preferredHeight: root.inputMinHeight
            onClicked: root.expandRequested()
        }

        Rectangle {
            id: composerFrame

            Layout.fillWidth: true
            Layout.preferredHeight: root.inputTargetHeight
            radius: root.compactMode ? 15 : 18
            color: root.disabledInput ? "#fff4ec" : "#fffdf8"
            border.color: root.disabledInput ? "#c66a4b" : (input.activeFocus ? "#9fb4cc" : "#cfd8e3")
            border.width: input.activeFocus || root.disabledInput ? 2 : 1
            clip: true

            TextArea {
                id: input

                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: root.buttonSize + 18
                anchors.topMargin: root.compactMode ? 5 : 8
                anchors.bottomMargin: root.compactMode ? 5 : 8
                wrapMode: TextArea.Wrap
                placeholderText: root.missingProviderConfig
                        ? "先点设置填写模型配置"
                        : (root.disconnectedInput ? "未连接，点重连或稍后重试" : "输入消息")
                placeholderTextColor: root.disabledInput ? "#8a4b38" : "#8d8580"
                color: enabled ? "#26201b" : "#6f5545"
                selectionColor: "#b9d0f2"
                selectedTextColor: "#26201b"
                enabled: root.canType
                textFormat: TextEdit.PlainText
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                background: Item {}

                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ShiftModifier) === 0) {
                        event.accepted = true
                        root.submitOrCancel()
                    }
                }
            }

            ToolButton {
                id: sendButton

                width: root.buttonSize
                height: root.buttonSize
                anchors.right: parent.right
                anchors.rightMargin: root.compactMode ? 7 : 9
                anchors.bottom: parent.bottom
                anchors.bottomMargin: root.compactMode ? 5 : 8
                enabled: root.canSubmit
                hoverEnabled: true
                text: ""
                padding: 0
                ToolTip.visible: !root.compactMode && hovered
                ToolTip.text: App.ChatController.sending ? "停止回复" : "发送"
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: App.ChatController.sending ? root.stopIconSize : root.sendIconSize
                        height: App.ChatController.sending ? root.stopIconSize : root.sendIconSize
                        source: App.ChatController.sending ? root.stopIconSource : root.sendIconSource
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit
                        opacity: sendButton.enabled ? 1.0 : 0.38
                        smooth: true
                    }
                }
                background: Rectangle {
                    radius: root.buttonSize / 2
                    color: App.ChatController.sending
                            ? (sendButton.down ? "#dfd5c9" : (sendButton.hovered ? "#f4ecdf" : "#eee8de"))
                            : (root.readyToSend
                            ? (sendButton.down ? "#244861" : (sendButton.hovered ? "#3b668a" : "#315a7d"))
                            : "#eee8de")
                    border.color: App.ChatController.sending
                            ? "#d1c6ba"
                            : (root.readyToSend ? (sendButton.hovered ? "#2d5576" : "#315a7d") : "#d8cec1")
                }
                onClicked: root.submitOrCancel()
            }
        }

        MilesIconButton {
            id: closeButton

            visible: root.compactMode
            iconSource: root.closeIconSource
            iconSize: root.sideIconSize
            tooltipText: ""
            showHoverFill: true
            Layout.preferredWidth: visible ? root.sideButtonWidth : 0
            Layout.preferredHeight: root.inputMinHeight
            onClicked: root.closeRequested()
        }
    }
}
