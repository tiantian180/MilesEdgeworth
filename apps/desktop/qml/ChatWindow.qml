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
    property string pendingDeleteConversationId: ""
    property string pendingDeleteConversationTitle: ""

    onVisibleChanged: App.DesktopShell.setChatWindowDockVisible(visible)

    function scheduleTranscriptScroll() {
        if (!transcriptScrollTimer.running) {
            transcriptScrollTimer.start()
        }
    }

    function setTranscriptProperty(index, name, value) {
        if (transcriptModel.get(index)[name] !== value) {
            transcriptModel.setProperty(index, name, value)
        }
    }

    function syncTranscriptMessages() {
        const messages = App.ChatController.messages
        const count = messages.length

        while (transcriptModel.count > count) {
            transcriptModel.remove(transcriptModel.count - 1)
        }

        for (let i = 0; i < count; ++i) {
            const message = messages[i]
            const next = {
                role: message.role || "",
                text: message.text || "",
                pending: message.pending === true,
                error: message.error === true,
                isPartial: message.isPartial === true
            }

            if (i >= transcriptModel.count) {
                transcriptModel.append(next)
                continue
            }

            setTranscriptProperty(i, "role", next.role)
            setTranscriptProperty(i, "text", next.text)
            setTranscriptProperty(i, "pending", next.pending)
            setTranscriptProperty(i, "error", next.error)
            setTranscriptProperty(i, "isPartial", next.isPartial)
        }

        scheduleTranscriptScroll()
    }

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
            chatWindow.syncTranscriptMessages()
        }
    }

    Component.onCompleted: syncTranscriptMessages()

    ListModel {
        id: transcriptModel
    }

    Timer {
        id: transcriptScrollTimer

        interval: 33
        repeat: false
        onTriggered: {
            transcript.positionViewAtEnd()
            if (App.ChatController.sending) {
                scheduleTranscriptScroll()
            }
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
                                onClicked: {
                                    chatWindow.pendingDeleteConversationId = modelData.id
                                    chatWindow.pendingDeleteConversationTitle = titleText
                                    deleteConversationDialog.open()
                                }
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
                        model: transcriptModel

                        delegate: Item {
                            required property string role
                            required property string text
                            required property bool pending
                            required property bool error
                            required property bool isPartial

                            readonly property bool partial: isPartial === true

                            width: transcript.width
                            height: bubble.implicitHeight + 4

                            Rectangle {
                                id: bubble

                                readonly property bool isUser: role === "user"
                                readonly property string bodyText: text.length > 0 ? text : "…"
                                readonly property real horizontalPadding: 16
                                readonly property real verticalPadding: 16
                                readonly property real maxBubbleWidth: parent.width * 0.82
                                readonly property real maxContentWidth: Math.max(1, maxBubbleWidth - horizontalPadding)

                                width: Math.min(maxBubbleWidth,
                                                Math.max(messageMeasure.contentWidth + horizontalPadding,
                                                         partial ? partialMetrics.width + horizontalPadding : 0))
                                implicitHeight: messageMeasure.contentHeight
                                                + (partialLabel.visible ? partialLabel.implicitHeight + 4 : 0)
                                                + verticalPadding
                                height: implicitHeight
                                anchors.right: isUser ? parent.right : undefined
                                anchors.left: isUser ? undefined : parent.left
                                radius: 6
                                color: error ? "#f6d6cc" : (isUser ? "#dce7f7" : "#eee7da")
                                border.color: error ? "#b65a45" : "transparent"

                                Text {
                                    id: messageMeasure

                                    visible: false
                                    width: bubble.maxContentWidth
                                    text: bubble.bodyText
                                    font.pixelSize: 14
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
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
                                    textMargin: 0
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

                        onCountChanged: chatWindow.scheduleTranscriptScroll()
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
                readonly property bool missingProviderConfig: disconnectedInput && !App.ChatController.providerConfigured

                Layout.fillWidth: true
                Layout.preferredHeight: 72
                wrapMode: TextArea.Wrap
                placeholderText: disconnectedInput
                        ? (missingProviderConfig ? "未连接，先点设置填写模型配置" : "未连接，点重连或稍后重试")
                        : "输入消息"
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

    Dialog {
        id: deleteConversationDialog

        modal: true
        anchors.centerIn: parent
        title: "删除会话"
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Label {
            width: 260
            text: chatWindow.pendingDeleteConversationTitle.length > 0
                    ? "确定删除「" + chatWindow.pendingDeleteConversationTitle + "」？"
                    : "确定删除这个会话？"
            color: "#26201b"
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            if (chatWindow.pendingDeleteConversationId.length > 0) {
                App.ChatController.deleteConversation(chatWindow.pendingDeleteConversationId)
            }
            chatWindow.pendingDeleteConversationId = ""
            chatWindow.pendingDeleteConversationTitle = ""
        }

        onRejected: {
            chatWindow.pendingDeleteConversationId = ""
            chatWindow.pendingDeleteConversationTitle = ""
        }
    }
}
