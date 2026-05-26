import QtQuick
import QtQuick.Controls
import MilesEdgeworth as App

ApplicationWindow {
    id: bubbleWindow

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
    property double hideDeadlineMs: 0
    property int remainingHideMs: 0
    readonly property int bubbleMargin: 12
    readonly property int contentHorizontalPadding: 28
    readonly property int contentVerticalPadding: 24
    readonly property int pointerExtent: 28
    readonly property int maxBubbleWidth: 360
    readonly property int minBubbleWidth: 210
    readonly property int bodyWidth: Math.max(minBubbleWidth,
            Math.min(maxBubbleWidth, messageMeasure.contentWidth + contentHorizontalPadding * 2))
    readonly property int bodyHeight: Math.max(82,
            Math.min(190, messageMeasure.contentHeight + contentVerticalPadding * 2))
    property string pointerPlacement: "bottomLeft"

    width: bodyWidth
    height: bodyHeight + pointerExtent
    onWidthChanged: updatePlacement()
    onHeightChanged: updatePlacement()

    function updatePlacement() {
        const nextPlacement = App.DesktopShell.placeChatBubble(width, height, bubbleMargin)
        x = nextPlacement.x || 0
        y = nextPlacement.y || 0
        pointerPlacement = nextPlacement.pointer || "bottomLeft"
        if (bubbleCanvas) {
            bubbleCanvas.requestPaint()
        }
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
        if (App.DesktopShell.chatWindowExpanded) {
            hideTimer.stop()
            visible = false
            return
        }

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
        const interval = Math.max(2500, Math.min(10000, 2500 + Math.ceil(assistantText.length / 20) * 1000))
        startHideTimer(interval)
    }

    function startHideTimer(interval) {
        remainingHideMs = Math.max(250, interval)
        hideTimer.interval = remainingHideMs
        hideDeadlineMs = Date.now() + remainingHideMs
        if (!bubbleHover.hovered) {
            hideTimer.restart()
        }
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

    Connections {
        target: App.DesktopShell

        function onChatWindowStateChanged() {
            bubbleWindow.syncAssistantBubble()
        }

        function onPetWindowGeometryChanged() {
            bubbleWindow.updatePlacement()
        }
    }

    Component.onCompleted: {
        updatePlacement()
        syncAssistantBubble()
    }

    Timer {
        id: hideTimer

        repeat: false
        onTriggered: {
            bubbleWindow.hideDeadlineMs = 0
            bubbleWindow.remainingHideMs = 0
            bubbleWindow.trackedAssistantMessageIndex = -1
            bubbleWindow.visible = false
        }
    }

    Item {
        id: bubbleFrame

        anchors.fill: parent

        HoverHandler {
            id: bubbleHover
            onHoveredChanged: {
                if (hovered) {
                    if (hideTimer.running) {
                        bubbleWindow.remainingHideMs = Math.max(250, Math.ceil(bubbleWindow.hideDeadlineMs - Date.now()))
                    }
                    hideTimer.stop()
                } else if (bubbleWindow.visible && bubbleWindow.assistantPending !== true) {
                    bubbleWindow.startHideTimer(bubbleWindow.remainingHideMs > 0
                            ? bubbleWindow.remainingHideMs
                            : 250)
                }
            }
        }

        Canvas {
            id: bubbleCanvas

            anchors.fill: parent
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                const w = bubbleWindow.width
                const h = bubbleWindow.height
                const tail = bubbleWindow.pointerExtent
                const bodyTop = bubbleWindow.pointerPlacement.startsWith("top") ? tail : 2
                const bodyBottom = bubbleWindow.pointerPlacement.startsWith("bottom") ? h - tail : h - 2
                const radius = 22
                const left = 4
                const right = w - 4
                const tailLeft = bubbleWindow.pointerPlacement.endsWith("Left")
                const tailBaseX = tailLeft ? 52 : w - 80
                const tailTipX = tailLeft ? 28 : w - 28
                const tailTipY = bubbleWindow.pointerPlacement.startsWith("top") ? 4 : h - 4

                ctx.clearRect(0, 0, w, h)
                ctx.beginPath()
                ctx.moveTo(left + radius, bodyTop)

                if (bubbleWindow.pointerPlacement === "topLeft"
                        || bubbleWindow.pointerPlacement === "topRight") {
                    ctx.lineTo(tailBaseX, bodyTop)
                    ctx.lineTo(tailTipX, tailTipY)
                    ctx.lineTo(tailBaseX + 28, bodyTop)
                }
                ctx.lineTo(right - radius, bodyTop)
                ctx.quadraticCurveTo(right, bodyTop, right, bodyTop + radius)
                ctx.lineTo(right, bodyBottom - radius)
                ctx.quadraticCurveTo(right, bodyBottom, right - radius, bodyBottom)
                if (bubbleWindow.pointerPlacement === "bottomRight"
                        || bubbleWindow.pointerPlacement === "bottomLeft") {
                    ctx.lineTo(tailBaseX + 28, bodyBottom)
                    ctx.lineTo(tailTipX, tailTipY)
                    ctx.lineTo(tailBaseX, bodyBottom)
                }
                ctx.lineTo(left + radius, bodyBottom)
                ctx.quadraticCurveTo(left, bodyBottom, left, bodyBottom - radius)
                ctx.lineTo(left, bodyTop + radius)
                ctx.quadraticCurveTo(left, bodyTop, left + radius, bodyTop)
                ctx.closePath()

                ctx.fillStyle = "#fffdf8"
                ctx.fill()
                ctx.lineWidth = 3
                ctx.strokeStyle = "#6f6a63"
                ctx.stroke()
            }

            Connections {
                target: bubbleWindow
                function onPointerPlacementChanged() { bubbleCanvas.requestPaint() }
            }
        }

        Text {
            id: messageMeasure

            visible: false
            width: bubbleWindow.maxBubbleWidth - bubbleWindow.contentHorizontalPadding * 2
            text: bubbleWindow.assistantText
            font.pixelSize: 14
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }

        Row {
            id: bubbleActions

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: bubbleWindow.pointerPlacement.startsWith("top") ? bubbleWindow.pointerExtent + 16 : 18
            anchors.rightMargin: 22
            spacing: 10
            opacity: bubbleHover.hovered ? 1 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: 100 }
            }

            ToolButton {
                id: expandButton

                text: "↗"
                font.pixelSize: 18
                width: 20
                height: 20
                ToolTip.visible: hovered
                ToolTip.text: "展开聊天"
                contentItem: Text {
                    text: expandButton.text
                    color: "#8d8780"
                    font: expandButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
                onClicked: App.ChatController.openWindow()
            }

            ToolButton {
                id: closeBubbleButton

                text: "×"
                font.pixelSize: 18
                width: 20
                height: 20
                ToolTip.visible: hovered
                ToolTip.text: "隐藏气泡"
                contentItem: Text {
                    text: closeBubbleButton.text
                    color: "#8d8780"
                    font: closeBubbleButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
                onClicked: bubbleWindow.hideCurrentBubble()
            }
        }

        ScrollView {
            id: bubbleScroll

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: bubbleWindow.contentHorizontalPadding
            anchors.rightMargin: bubbleWindow.contentHorizontalPadding + 60
            anchors.topMargin: bubbleWindow.pointerPlacement.startsWith("top")
                    ? bubbleWindow.pointerExtent + bubbleWindow.contentVerticalPadding
                    : bubbleWindow.contentVerticalPadding
            anchors.bottomMargin: bubbleWindow.pointerPlacement.startsWith("bottom")
                    ? bubbleWindow.pointerExtent + bubbleWindow.contentVerticalPadding
                    : bubbleWindow.contentVerticalPadding
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
