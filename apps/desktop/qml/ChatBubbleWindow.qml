import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MilesEdgeworth as App

ApplicationWindow {
    id: bubbleWindow

    width: 340
    height: Math.min(180, Math.max(78, messageMeasure.contentHeight + 58))
    visible: false
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    title: "Miles Reply"

    property string assistantText: ""
    property bool assistantPending: false
    property bool bubbleDismissed: false

    x: Math.round(App.DesktopShell.petWindowX
                  + App.DesktopShell.petWindowWidth / 2
                  - width / 2)
    y: Math.round(App.DesktopShell.petWindowY - height - 10)

    function syncAssistantBubble() {
        const messages = App.ChatController.messages
        let found = false
        let nextText = ""
        let nextPending = false

        for (let i = messages.length - 1; i >= 0; --i) {
            const message = messages[i]
            if (message.role === "assistant") {
                nextText = message.text || ""
                nextPending = message.pending === true
                found = true
                break
            }
        }

        if (!found || nextText.length === 0) {
            assistantText = ""
            assistantPending = false
            hideTimer.stop()
            visible = false
            return
        }

        const previousText = assistantText
        assistantText = nextText
        assistantPending = nextPending

        if (nextText.length > previousText.length || nextText !== previousText) {
            bubbleDismissed = false
        }

        if (!bubbleDismissed) {
            visible = true
            raise()
        }

        if (assistantPending === true) {
            hideTimer.stop()
            return
        }

        scheduleHide()
    }

    function scheduleHide() {
        hideTimer.interval = Math.max(4000, Math.min(12000, 3000 + assistantText.length * 80))
        hideTimer.restart()
    }

    Connections {
        target: App.ChatController

        function onMessagesChanged() {
            bubbleWindow.syncAssistantBubble()
        }

        function onOpenWindowRequested() {
            bubbleWindow.hideTimer.stop()
        }
    }

    Component.onCompleted: syncAssistantBubble()

    Timer {
        id: hideTimer

        repeat: false
        onTriggered: bubbleWindow.visible = false
    }

    Rectangle {
        id: bubbleFrame

        anchors.fill: parent
        radius: 8
        color: "#fffdf8"
        border.color: "#bfae9e"
        border.width: 1

        HoverHandler {
            id: bubbleHover
        }

        Text {
            id: messageMeasure

            visible: false
            width: bubbleText.width
            text: bubbleWindow.assistantText
            font.pixelSize: 14
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }

        RowLayout {
            id: bubbleActions

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 6
            anchors.rightMargin: 6
            spacing: 4

            ToolButton {
                id: expandButton

                text: "展开"
                font.pixelSize: 12
                Layout.preferredWidth: 44
                Layout.preferredHeight: 26
                contentItem: Text {
                    text: expandButton.text
                    color: "#5a4031"
                    font: expandButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: expandButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#d8d1c8"
                }
                onClicked: App.ChatController.openWindow()
            }

            ToolButton {
                id: closeBubbleButton

                visible: bubbleHover.hovered
                text: "关闭"
                font.pixelSize: 12
                Layout.preferredWidth: visible ? 44 : 0
                Layout.preferredHeight: 26
                contentItem: Text {
                    text: closeBubbleButton.text
                    color: "#5a4031"
                    font: closeBubbleButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: closeBubbleButton.down ? "#e1d8ce" : "#f3ede5"
                    border.color: "#d8d1c8"
                }
                onClicked: {
                    bubbleWindow.bubbleDismissed = true
                    bubbleWindow.hideTimer.stop()
                    bubbleWindow.visible = false
                }
            }
        }

        ScrollView {
            id: bubbleScroll

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 36
            anchors.bottomMargin: 12
            clip: true

            TextArea {
                id: bubbleText

                text: bubbleWindow.assistantText
                readOnly: true
                selectByMouse: true
                wrapMode: TextArea.Wrap
                textFormat: TextEdit.PlainText
                color: "#26201b"
                selectedTextColor: "#26201b"
                selectionColor: "#b9d0f2"
                font.pixelSize: 14
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                background: Item {}

                onTextChanged: cursorPosition = text.length
            }
        }
    }
}
