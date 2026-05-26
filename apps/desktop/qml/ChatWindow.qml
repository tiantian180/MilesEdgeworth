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
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "Miles Chat"
    color: "transparent"

    property bool conversationPanelOpen: false
    property string pendingDeleteConversationId: ""
    property string pendingDeleteConversationTitle: ""
    property bool compactMode: false
    property int expandedWidth: 420
    property int expandedHeight: 560
    property int compactWidth: 380
    property int compactMinHeight: 56
    property string draftText: ""
    readonly property int expandedTitleButtonInset: 11

    onVisibleChanged: {
        App.DesktopShell.setChatWindowDockVisible(visible)
        syncShellChatState()
    }

    onCompactModeChanged: syncShellChatState()

    function syncShellChatState() {
        App.DesktopShell.setChatWindowExpanded(visible && !compactMode)
    }

    function syncDraftFromVisibleComposer() {
        draftText = compactMode ? compactComposer.text : expandedComposer.text
    }

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
        showExpanded()
        App.ChatController.loadConversations()
        show()
        raise()
        requestActivate()
        expandedComposer.forceInputFocus()
    }

    function showExpanded() {
        if (compactMode) {
            syncDraftFromVisibleComposer()
        }
        compactMode = false
        minimumWidth = 360
        minimumHeight = 420
        maximumHeight = 16777215
        width = Math.max(360, expandedWidth)
        height = Math.max(420, expandedHeight)
        expandedComposer.text = draftText
        syncShellChatState()
    }

    function showCompact() {
        if (!compactMode) {
            expandedWidth = width
            expandedHeight = height
            syncDraftFromVisibleComposer()
        }
        conversationPanelOpen = false
        compactMode = true
        minimumWidth = compactWidth
        minimumHeight = compactMinHeight
        maximumHeight = compactComposer.implicitHeight
        width = compactWidth
        height = compactComposer.implicitHeight
        compactComposer.text = draftText
        syncShellChatState()
        compactComposer.forceInputFocus()
    }

    function hideChatUi() {
        hide()
        syncShellChatState()
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
        anchors.margins: 0
        spacing: 0

        Rectangle {
            id: expandedShell

            visible: !chatWindow.compactMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 22
            color: "#fffaf2"
            border.color: "#d8cec1"
            border.width: 1
            antialiasing: true
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    id: expandedTitleBar

                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    color: "transparent"

                    MouseArea {
                        id: expandedTitleDragArea

                        anchors.fill: parent
                        z: 0
                        acceptedButtons: Qt.LeftButton
                        onPressed: chatWindow.startSystemMove()
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: chatWindow.expandedTitleButtonInset
                        anchors.rightMargin: chatWindow.expandedTitleButtonInset
                        z: 1
                        spacing: 8

                        MilesIconButton {
                            iconSource: "qrc:/ui-icons/menu.svg"
                            iconSize: 15
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            tooltipText: "会话"
                            onClicked: chatWindow.conversationPanelOpen = !chatWindow.conversationPanelOpen
                        }

                        Label {
                            text: "Miles"
                            color: "#2d2925"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: App.ChatController.sidecarReady ? "#4d985d" : "#b65a45"
                        }

                        Label {
                            text: App.ChatController.statusText
                            color: App.ChatController.sidecarReady ? "#557c59" : "#8a4b38"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.maximumWidth: 128
                        }

                        Item { Layout.fillWidth: true }

                        MilesIconButton {
                            iconSource: "qrc:/ui-icons/settings.svg"
                            iconSize: 14
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            tooltipText: "设置"
                            onClicked: App.SettingsController.openWindow()
                        }

                        MilesIconButton {
                            iconSource: "qrc:/ui-icons/refresh-ccw.svg"
                            iconSize: 14
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            tooltipText: "重连"
                            enabled: !App.ChatController.sending
                            onClicked: {
                                App.ChatController.startSidecar()
                                App.ChatController.checkHealth()
                            }
                        }

                        MilesIconButton {
                            iconSource: "qrc:/ui-icons/minimize-2.svg"
                            iconSize: 14
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            tooltipText: "收起"
                            onClicked: chatWindow.showCompact()
                        }

                        MilesIconButton {
                            iconSource: "qrc:/ui-icons/x.svg"
                            iconSize: 14
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            tooltipText: "隐藏聊天"
                            onClicked: chatWindow.hideChatUi()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 26
                    Layout.rightMargin: 26
                    spacing: chatWindow.conversationPanelOpen ? 10 : 0
                    visible: !chatWindow.compactMode
                    Layout.preferredHeight: chatWindow.compactMode ? 0 : -1

                    Rectangle {
                        Layout.preferredWidth: chatWindow.conversationPanelOpen ? 168 : 0
                        Layout.minimumWidth: chatWindow.conversationPanelOpen ? 168 : 0
                        Layout.maximumWidth: chatWindow.conversationPanelOpen ? 168 : 0
                        Layout.fillHeight: true
                        clip: true
                        color: "#f3ede5"
                        border.color: chatWindow.conversationPanelOpen ? "#d8d1c8" : "transparent"
                        radius: 12
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
                            radius: 0
                            color: "#fffaf2"
                            border.color: "transparent"

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
                                        readonly property string bodyText: text.length > 0 ? text : "..."
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
                                        radius: 13
                                        color: error ? "#f6d6cc" : (isUser ? "#dcecff" : "#fffdf8")
                                        border.color: error ? "#b65a45" : (isUser ? "transparent" : "#ded4c8")

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

                Rectangle {
                    id: expandedComposerArea

                    Layout.fillWidth: true
                    Layout.preferredHeight: expandedComposer.implicitHeight + 18
                    color: "transparent"
                    border.color: "transparent"
                    border.width: 0

                    ChatComposer {
                        id: expandedComposer

                        anchors.fill: parent
                        anchors.leftMargin: 26
                        anchors.rightMargin: 26
                        anchors.bottomMargin: 12
                        compactMode: false

                        onSubmitRequested: function(text) {
                            clearText()
                            chatWindow.draftText = ""
                            App.ChatController.sendMessage(text)
                        }

                        onTextChanged: chatWindow.draftText = text
                        onCancelRequested: App.ChatController.cancelCurrentReply()
                        onExpandRequested: App.ChatController.openWindow()
                        onCloseRequested: chatWindow.hideChatUi()
                    }
                }
            }
        }

        Rectangle {
            id: compactDragShell

            visible: chatWindow.compactMode
            Layout.fillWidth: true
            Layout.preferredHeight: compactComposer.implicitHeight
            radius: 18
            color: "#fffaf2"
            border.color: "#d8cec1"
            border.width: 1
            antialiasing: true

            MouseArea {
                id: compactDragArea

                anchors.fill: parent
                z: 0
                enabled: chatWindow.compactMode
                acceptedButtons: Qt.LeftButton
                onPressed: chatWindow.startSystemMove()
            }

            ChatComposer {
                id: compactComposer

                anchors.fill: parent
                z: 1
                compactMode: chatWindow.compactMode

                onSubmitRequested: function(text) {
                    clearText()
                    chatWindow.draftText = ""
                    App.ChatController.sendMessage(text)
                }

                onTextChanged: chatWindow.draftText = text
                onCancelRequested: App.ChatController.cancelCurrentReply()
                onExpandRequested: App.ChatController.openWindow()
                onCloseRequested: chatWindow.hideChatUi()

                onImplicitHeightChanged: {
                    if (chatWindow.compactMode) {
                        chatWindow.maximumHeight = implicitHeight
                        chatWindow.height = implicitHeight
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteConversationDialog

        modal: true
        anchors.centerIn: parent
        width: 320
        title: "删除会话"
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Label {
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
