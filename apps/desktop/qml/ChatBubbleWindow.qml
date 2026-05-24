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
    property bool suppressNextAssistantBubble: false
    property int assistantMessageIndex: -1
    property int dismissedAssistantMessageIndex: -1
    property int suppressedAssistantMessageIndex: -1
    property int trackedAssistantMessageIndex: -1
    readonly property int screenMargin: 10

    x: clampedBubbleX()
    y: clampedBubbleY()

    function clamp(value, minimum, maximum) {
        return maximum >= minimum ? Math.min(Math.max(value, minimum), maximum) : minimum
    }

    function clampedBubbleX() {
        const preferredX = App.DesktopShell.petWindowX
                + App.DesktopShell.petWindowWidth / 2
                - width / 2
        return Math.round(clamp(preferredX,
                                App.DesktopShell.petScreenAvailableX + screenMargin,
                                App.DesktopShell.petScreenAvailableX
                                + App.DesktopShell.petScreenAvailableWidth
                                - width
                                - screenMargin))
    }

    function clampedBubbleY() {
        const preferredY = App.DesktopShell.petWindowY - height - screenMargin
        return Math.round(clamp(preferredY,
                                App.DesktopShell.petScreenAvailableY + screenMargin,
                                App.DesktopShell.petScreenAvailableY
                                + App.DesktopShell.petScreenAvailableHeight
                                - height
                                - screenMargin))
    }

    function latestAssistantMessageIndex() {
        const messages = App.ChatController.messages
        for (let i = messages.length - 1; i >= 0; --i) {
            if (messages[i].role === "assistant") {
                return i
            }
        }

        return -1
    }

    function syncAssistantBubble() {
        const messages = App.ChatController.messages
        let found = false
        let nextText = ""
        let nextPending = false
        let nextAssistantMessageIndex = -1

        for (let i = messages.length - 1; i >= 0; --i) {
            const message = messages[i]
            if (message.role === "assistant") {
                nextText = message.text || ""
                nextPending = message.pending === true
                nextAssistantMessageIndex = i
                found = true
                break
            }
        }

        if (!found) {
            assistantText = ""
            assistantPending = false
            assistantMessageIndex = -1
            dismissedAssistantMessageIndex = -1
            suppressedAssistantMessageIndex = -1
            trackedAssistantMessageIndex = -1
            bubbleDismissed = false
            suppressNextAssistantBubble = App.ChatController.sending === true && suppressNextAssistantBubble
            hideTimer.stop()
            visible = false
            return
        }

        const assistantMessageChanged = nextAssistantMessageIndex !== assistantMessageIndex
        if (assistantMessageChanged) {
            assistantMessageIndex = nextAssistantMessageIndex
            dismissedAssistantMessageIndex = -1
            suppressedAssistantMessageIndex = -1
            trackedAssistantMessageIndex = -1
            bubbleDismissed = false

            if (suppressNextAssistantBubble && nextPending === true) {
                suppressedAssistantMessageIndex = nextAssistantMessageIndex
                bubbleDismissed = true
                suppressNextAssistantBubble = false
            } else if (suppressNextAssistantBubble) {
                suppressNextAssistantBubble = false
            }
        }

        if (nextPending === true) {
            trackedAssistantMessageIndex = nextAssistantMessageIndex
        }

        assistantText = nextText
        assistantPending = nextPending

        if (nextText.length === 0) {
            hideTimer.stop()
            visible = false
            return
        }

        if (assistantPending !== true && trackedAssistantMessageIndex !== nextAssistantMessageIndex) {
            trackedAssistantMessageIndex = -1
            hideTimer.stop()
            visible = false
            return
        }

        if (bubbleDismissed
                || dismissedAssistantMessageIndex === nextAssistantMessageIndex
                || suppressedAssistantMessageIndex === nextAssistantMessageIndex) {
            hideTimer.stop()
            visible = false
            return
        }

        visible = true
        raise()

        if (assistantPending === true) {
            hideTimer.stop()
            return
        }

        scheduleHide()
    }

    function hideCurrentBubble() {
        const index = latestAssistantMessageIndex()
        if (index >= 0) {
            assistantMessageIndex = index
            dismissedAssistantMessageIndex = index
            suppressedAssistantMessageIndex = index
            bubbleDismissed = true
            suppressNextAssistantBubble = false
        } else if (App.ChatController.sending === true) {
            suppressNextAssistantBubble = true
        }

        trackedAssistantMessageIndex = -1
        hideTimer.stop()
        visible = false
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
            bubbleWindow.hideCurrentBubble()
        }

        function onSendingChanged() {
            if (App.ChatController.sending !== true) {
                bubbleWindow.suppressNextAssistantBubble = false
            }
            bubbleWindow.syncAssistantBubble()
        }
    }

    Component.onCompleted: syncAssistantBubble()

    Timer {
        id: hideTimer

        repeat: false
        onTriggered: {
            bubbleWindow.trackedAssistantMessageIndex = -1
            bubbleWindow.visible = false
        }
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

                visible: true
                enabled: bubbleHover.hovered
                opacity: bubbleHover.hovered ? 1 : 0
                text: "关闭"
                font.pixelSize: 12
                Layout.preferredWidth: 44
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
                onClicked: bubbleWindow.hideCurrentBubble()
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

                width: bubbleScroll.availableWidth
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

                onTextChanged: {
                    if (text.length > cursorPosition && !bubbleHover.hovered && !activeFocus) {
                        cursorPosition = text.length
                    }
                }
            }
        }
    }
}
