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
    readonly property int contentHorizontalPadding: 24
    readonly property int contentVerticalPadding: 22
    readonly property int pointerExtent: 20
    readonly property int maxBubbleWidth: 320
    readonly property int minBubbleWidth: 190
    readonly property int maxBubbleHeight: 340
    readonly property int maxBodyHeight: maxBubbleHeight - pointerExtent
    readonly property bool bodyAtMaxHeight: messageMeasure.contentHeight + contentVerticalPadding * 2 >= maxBodyHeight
    readonly property int bodyWidth: Math.max(minBubbleWidth,
            Math.min(maxBubbleWidth, messageMeasure.contentWidth + contentHorizontalPadding * 2))
    readonly property int bodyHeight: Math.max(76,
            Math.min(maxBodyHeight, messageMeasure.contentHeight + contentVerticalPadding * 2))
    readonly property int bodyTop: pointerPlacement.startsWith("top") ? pointerExtent : 2
    readonly property int bodyBottom: pointerPlacement.startsWith("bottom") ? height - pointerExtent : height - 2
    property string pointerPlacement: "bottomLeft"
    property int tailX: Math.round(width / 2)

    width: bodyWidth
    height: bodyHeight + pointerExtent
    onWidthChanged: updatePlacement()
    onHeightChanged: updatePlacement()

    function updatePlacement() {
        const nextPlacement = App.DesktopShell.placeChatBubble(width, height, bubbleMargin)
        x = nextPlacement.x || 0
        y = nextPlacement.y || 0
        pointerPlacement = nextPlacement.pointer || "bottomLeft"
        tailX = Math.max(38, Math.min(width - 38, nextPlacement.tailX || Math.round(width / 2)))
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

    function hideForExpandedChat() {
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
            bubbleWindow.hideForExpandedChat()
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
                const tailX = Math.max(38, Math.min(w - 38, bubbleWindow.tailX))
                const tailHalf = 10
                const bodyTop = bubbleWindow.bodyTop
                const bodyBottom = bubbleWindow.bodyBottom
                const radius = 22
                const left = 4
                const right = w - 4

                ctx.clearRect(0, 0, w, h)
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.moveTo(left + radius, bodyTop)

                if (bubbleWindow.pointerPlacement === "topLeft"
                        || bubbleWindow.pointerPlacement === "topRight") {
                    ctx.lineTo(tailX - tailHalf, bodyTop)
                    ctx.lineTo(tailX, 4)
                    ctx.lineTo(tailX + tailHalf, bodyTop)
                }
                ctx.lineTo(right - radius, bodyTop)
                ctx.quadraticCurveTo(right, bodyTop, right, bodyTop + radius)
                ctx.lineTo(right, bodyBottom - radius)
                ctx.quadraticCurveTo(right, bodyBottom, right - radius, bodyBottom)
                if (bubbleWindow.pointerPlacement === "bottomRight"
                        || bubbleWindow.pointerPlacement === "bottomLeft") {
                    ctx.lineTo(tailX + tailHalf, bodyBottom)
                    ctx.lineTo(tailX, h - 4)
                    ctx.lineTo(tailX - tailHalf, bodyBottom)
                }
                ctx.lineTo(left + radius, bodyBottom)
                ctx.quadraticCurveTo(left, bodyBottom, left, bodyBottom - radius)
                ctx.lineTo(left, bodyTop + radius)
                ctx.quadraticCurveTo(left, bodyTop, left + radius, bodyTop)
                ctx.closePath()

                ctx.fillStyle = "#fffdf8"
                ctx.fill()
                ctx.lineWidth = 2.4
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
            anchors.topMargin: bubbleWindow.bodyTop + 8
            anchors.rightMargin: 8
            spacing: 5
            z: 2
            opacity: bubbleHover.hovered ? 0.82 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: 100 }
            }

            MilesIconButton {
                id: expandButton

                iconSource: "qrc:/ui-icons/maximize-2.svg"
                iconSize: 9
                width: 14
                height: 14
                implicitWidth: 14
                implicitHeight: 14
                tooltipText: ""
                showHoverFill: true
                onClicked: App.ChatController.openWindow()
            }

            MilesIconButton {
                id: closeBubbleButton

                iconSource: "qrc:/ui-icons/x.svg"
                iconSize: 11
                width: 14
                height: 14
                implicitWidth: 14
                implicitHeight: 14
                tooltipText: ""
                showHoverFill: true
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
            anchors.rightMargin: bubbleWindow.contentHorizontalPadding
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
                    if (!bubbleHover.hovered && !activeFocus) {
                        cursorPosition = bubbleWindow.bodyAtMaxHeight ? text.length : 0
                    }
                }
            }
        }
    }
}
