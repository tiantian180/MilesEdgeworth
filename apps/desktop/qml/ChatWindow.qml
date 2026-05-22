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

    property bool conversationPanelOpen: false

    onVisibleChanged: App.DesktopShell.setChatWindowDockVisible(visible)

    function open() {
        App.ChatController.loadConversations()
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

            ToolButton {
                id: conversationToggleButton

                text: "☰"
                font.pixelSize: 16
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                contentItem: Text {
                    text: conversationToggleButton.text
                    color: "#26201b"
                    font: conversationToggleButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: conversationToggleButton.down || chatWindow.conversationPanelOpen ? "#eee7da" : "#f3ede5"
                    border.color: chatWindow.conversationPanelOpen ? "#bfae9e" : "#d8d1c8"
                }
                ToolTip.text: "会话"
                onClicked: chatWindow.conversationPanelOpen = !chatWindow.conversationPanelOpen
            }

            Label {
                text: "Miles"
                color: "#26201b"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }

            Label {
                text: App.ChatController.statusText
                color: App.ChatController.sidecarReady ? "#386641" : "#8a4b38"
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.maximumWidth: 112
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: chatWindow.conversationPanelOpen ? 10 : 0

            Rectangle {
                Layout.preferredWidth: chatWindow.conversationPanelOpen ? 168 : 0
                Layout.minimumWidth: chatWindow.conversationPanelOpen ? 168 : 0
                Layout.maximumWidth: chatWindow.conversationPanelOpen ? 168 : 0
                Layout.fillHeight: true
                clip: true
                color: "#eee7da"
                border.color: chatWindow.conversationPanelOpen ? "#d8d1c8" : "transparent"
                radius: 6
                visible: chatWindow.conversationPanelOpen

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Button {
                        id: newConversationButton

                        text: "新建"
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        contentItem: Text {
                            text: newConversationButton.text
                            color: "#26201b"
                            font: newConversationButton.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: newConversationButton.down ? "#d8d1c8" : "#fffdf8"
                            border.color: "#bfae9e"
                        }
                        onClicked: App.ChatController.newConversation()
                    }

                    ListView {
                        id: conversationsList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: App.ChatController.conversations

                        delegate: Rectangle {
                            required property var modelData

                            readonly property bool current: modelData.isCurrent === true
                                    || modelData.id === App.ChatController.currentConversationId
                            readonly property string titleText: modelData.title && modelData.title.length > 0
                                    ? modelData.title : "未命名会话"
                            readonly property string updatedText: modelData.updatedAt && modelData.updatedAt.length > 0
                                    ? modelData.updatedAt : ""

                            width: conversationsList.width
                            height: 74
                            radius: 6
                            color: current ? "#fffdf8" : "transparent"
                            border.color: current ? "#bfae9e" : "transparent"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: App.ChatController.switchConversation(modelData.id)
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: deleteConversationButton.left
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 8
                                anchors.rightMargin: 6
                                spacing: 4

                                Label {
                                    text: titleText
                                    color: "#26201b"
                                    font.pixelSize: 13
                                    font.weight: current ? Font.DemiBold : Font.Normal
                                    width: parent.width
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: updatedText
                                    color: "#6f6258"
                                    font.pixelSize: 11
                                    width: parent.width
                                    elide: Text.ElideRight
                                }
                            }

                            Button {
                                id: deleteConversationButton

                                text: "删"
                                font.pixelSize: 12
                                width: 32
                                height: 28
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                contentItem: Text {
                                    text: deleteConversationButton.text
                                    color: "#5a4031"
                                    font: deleteConversationButton.font
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: deleteConversationButton.down ? "#d8d1c8" : "#f7f4ef"
                                    border.color: "#d8d1c8"
                                }
                                onClicked: App.ChatController.deleteConversation(modelData.id)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: App.ChatController.conversationSkinMismatch ? skinWarningText.implicitHeight + 16 : 0
                    visible: App.ChatController.conversationSkinMismatch
                    radius: 6
                    color: "#fff4ec"
                    border.color: "#bfae9e"
                    clip: true

                    Label {
                        id: skinWarningText

                        anchors.fill: parent
                        anchors.margins: 8
                        text: App.ChatController.conversationSkinHint
                        color: "#5a4031"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        verticalAlignment: Text.AlignVCenter
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

                            readonly property bool partial: modelData.isPartial === true

                            width: transcript.width
                            height: bubble.implicitHeight + 4

                            Rectangle {
                                id: bubble

                                readonly property bool isUser: modelData.role === "user"
                                readonly property string bodyText: modelData.text.length > 0 ? modelData.text : "…"

                                width: Math.min(parent.width * 0.82,
                                                Math.max(44, messageMetrics.width + 24,
                                                         partial ? partialMetrics.width + 16 : 0))
                                implicitHeight: messageText.contentHeight + (partialLabel.visible ? partialLabel.implicitHeight + 4 : 0) + 16
                                height: implicitHeight
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

                                TextMetrics {
                                    id: partialMetrics

                                    text: "已截断"
                                    font: partialLabel.font
                                }

                                TextEdit {
                                    id: messageText

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 8
                                    height: contentHeight
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

                                Label {
                                    id: partialLabel

                                    visible: partial
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: messageText.bottom
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    anchors.topMargin: 4
                                    text: "已截断"
                                    color: "#6f6258"
                                    font.pixelSize: 11
                                }
                            }
                        }

                        onCountChanged: Qt.callLater(function() {
                            transcript.positionViewAtEnd()
                        })
                    }
                }
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
