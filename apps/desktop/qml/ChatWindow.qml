import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MilesEdgeworth as App

ApplicationWindow {
    id: chatWindow

    width: 420
    height: 560
    minimumWidth: 360
    minimumHeight: 420
    visible: false
    flags: Qt.Window
    title: "Miles Chat"
    color: "#f7f4ef"

    onVisibleChanged: App.DesktopShell.setChatWindowDockVisible(visible)

    function open() {
        show()
        raise()
        requestActivate()
        input.forceActiveFocus()
    }

    Connections {
        target: App.ChatController
        function onOpenWindowRequested() {
            chatWindow.open()
        }

        function onMessagesChanged() {
            Qt.callLater(function() {
                transcript.positionViewAtEnd()
            })
        }
    }

    ColumnLayout {
        id: chatLayout

        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        function submitInput() {
            if (App.ChatController.sending) {
                App.ChatController.cancelCurrentReply()
                return
            }

            const text = input.text.trim()
            if (text.length === 0) {
                return
            }
            input.text = ""
            App.ChatController.sendMessage(text)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Miles"
                color: "#26201b"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Label {
                text: App.ChatController.statusText
                color: App.ChatController.sidecarReady ? "#386641" : "#8a4b38"
                font.pixelSize: 12
            }

            Button {
                id: settingsButton

                text: "设置"
                font.pixelSize: 13
                Layout.preferredWidth: 76
                Layout.preferredHeight: 36
                contentItem: Text {
                    text: settingsButton.text
                    color: settingsButton.enabled ? "#5a4031" : "#9a9086"
                    font: settingsButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: settingsButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#bfae9e"
                }
                onClicked: App.SettingsController.openWindow()
            }

            Button {
                id: reconnectButton

                text: "重连"
                enabled: !App.ChatController.sending
                font.pixelSize: 13
                Layout.preferredWidth: 76
                Layout.preferredHeight: 36
                contentItem: Text {
                    text: reconnectButton.text
                    color: reconnectButton.enabled ? "#5a4031" : "#9a9086"
                    font: reconnectButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: reconnectButton.enabled ? (reconnectButton.down ? "#e1d8ce" : "#f3ede5") : "#eee9e2"
                    border.color: reconnectButton.enabled ? "#bfae9e" : "#d8d1c8"
                }
                onClicked: {
                    App.ChatController.startSidecar()
                    App.ChatController.checkHealth()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#fffdf8"
            border.color: "#d8d1c8"

            ListView {
                id: transcript

                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 8
                model: App.ChatController.messages

                delegate: Item {
                    required property var modelData

                    width: transcript.width
                    height: bubble.implicitHeight + 4

                    Rectangle {
                        id: bubble

                        readonly property bool isUser: modelData.role === "user"
                        readonly property string bodyText: modelData.text.length > 0 ? modelData.text : "…"

                        width: Math.min(parent.width * 0.82, Math.max(44, messageMetrics.width + 24))
                        implicitHeight: messageText.contentHeight + 16
                        anchors.right: isUser ? parent.right : undefined
                        anchors.left: isUser ? undefined : parent.left
                        radius: 6
                        color: modelData.error ? "#f6d6cc" : (isUser ? "#dce7f7" : "#eee7da")
                        border.color: modelData.error ? "#b65a45" : "transparent"

                        TextMetrics {
                            id: messageMetrics

                            text: bubble.bodyText
                            font: messageText.font
                        }

                        TextEdit {
                            id: messageText

                            anchors.fill: parent
                            anchors.margins: 8
                            text: bubble.bodyText
                            color: "#26201b"
                            font.pixelSize: 14
                            readOnly: true
                            selectByMouse: true
                            selectByKeyboard: true
                            selectedTextColor: "#26201b"
                            selectionColor: "#b9d0f2"
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.Wrap
                        }
                    }
                }

                onCountChanged: Qt.callLater(function() {
                    transcript.positionViewAtEnd()
                })
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextArea {
                id: input

                readonly property bool disconnectedInput: !App.ChatController.sidecarReady && !App.ChatController.sending

                Layout.fillWidth: true
                Layout.preferredHeight: 72
                wrapMode: TextArea.Wrap
                placeholderText: disconnectedInput ? "未连接，先点设置填写模型配置" : "输入消息"
                placeholderTextColor: disconnectedInput ? "#8a4b38" : "#82786e"
                color: enabled ? "#26201b" : "#6f5545"
                selectionColor: "#b9d0f2"
                selectedTextColor: "#26201b"
                enabled: App.ChatController.sidecarReady && !App.ChatController.sending
                opacity: 1.0
                leftPadding: 12
                rightPadding: 12
                topPadding: 10
                bottomPadding: 10
                background: Rectangle {
                    radius: 6
                    color: input.disconnectedInput ? "#fff4ec" : (input.enabled ? "#fffdf8" : "#eee9e2")
                    border.color: input.disconnectedInput ? "#c66a4b" : (input.activeFocus ? "#7b604c" : "#d8d1c8")
                    border.width: (input.disconnectedInput || input.activeFocus) ? 2 : 1
                }

                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ShiftModifier) === 0) {
                        event.accepted = true
                        chatLayout.submitInput()
                    }
                }
            }

            Button {
                id: sendButton

                text: App.ChatController.sending ? "停止" : "发送"
                enabled: App.ChatController.sending || (App.ChatController.sidecarReady && input.text.trim().length > 0)
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.preferredWidth: 76
                Layout.preferredHeight: 72
                contentItem: Text {
                    text: sendButton.text
                    color: sendButton.enabled ? "#fffdf8" : "#9a9086"
                    font: sendButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: sendButton.enabled ? (sendButton.down ? "#4b372b" : "#6f4f3e") : "#eee9e2"
                    border.color: sendButton.enabled ? "#5a4031" : "#d8d1c8"
                }

                onClicked: chatLayout.submitInput()
            }
        }
    }
}
