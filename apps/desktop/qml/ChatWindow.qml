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
    title: "Miles Chat"
    color: "#f7f4ef"

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

                        width: Math.min(parent.width * 0.82, messageText.implicitWidth + 24)
                        implicitHeight: messageText.implicitHeight + 16
                        anchors.right: isUser ? parent.right : undefined
                        anchors.left: isUser ? undefined : parent.left
                        radius: 6
                        color: modelData.error ? "#f6d6cc" : (isUser ? "#dce7f7" : "#eee7da")
                        border.color: modelData.error ? "#b65a45" : "transparent"

                        Text {
                            id: messageText

                            anchors.fill: parent
                            anchors.margins: 8
                            text: modelData.text.length > 0 ? modelData.text : "…"
                            color: "#26201b"
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
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

                Layout.fillWidth: true
                Layout.preferredHeight: 72
                wrapMode: TextArea.Wrap
                placeholderText: "输入消息"
                placeholderTextColor: "#82786e"
                color: "#26201b"
                selectionColor: "#b9d0f2"
                selectedTextColor: "#26201b"
                enabled: App.ChatController.sidecarReady && !App.ChatController.sending
                leftPadding: 12
                rightPadding: 12
                topPadding: 10
                bottomPadding: 10
                background: Rectangle {
                    radius: 6
                    color: input.enabled ? "#fffdf8" : "#eee9e2"
                    border.color: input.activeFocus ? "#7b604c" : "#d8d1c8"
                    border.width: input.activeFocus ? 2 : 1
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
