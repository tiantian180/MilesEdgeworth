import QtQuick
import QtQuick.Window
import Qt.labs.platform as Platform
import QtMultimedia
import MilesEdgeworth as App

Window {
    id: petWindow

    width: 240
    height: 240
    visible: true
    color: "transparent"
    title: "MilesEdgeworth v2"

    flags: Qt.FramelessWindowHint
           | Qt.NoDropShadowWindowHint

    // Phase 0 先使用平台原生菜单承载最小操作入口。
    // 这样菜单的 hover、外部点击关闭、阴影和系统质感都交给 Qt/系统处理。
    Platform.Menu {
        id: contextMenu

        Platform.MenuItem {
            text: App.DesktopShell.alwaysOnTop ? "取消置顶" : "始终置顶"
            onTriggered: App.DesktopShell.toggleAlwaysOnTop()
        }

        Platform.MenuItem {
            text: "禁止走动"
            checkable: true
            checked: !App.PetRuntime.autoMovementEnabled
            onTriggered: App.PetRuntime.toggleAutoMovementEnabled()
        }

        Platform.MenuItem {
            text: "静音"
            checkable: true
            checked: App.PetRuntime.audioMuted
            onTriggered: App.PetRuntime.toggleAudioMuted()
        }

        Platform.Menu {
            title: "语音语言"

            Platform.MenuItem {
                text: "日语"
                checkable: true
                checked: App.PetRuntime.voiceLanguage === "jp"
                onTriggered: App.PetRuntime.setVoiceLanguage("jp")
            }

            Platform.MenuItem {
                text: "英语"
                checkable: true
                checked: App.PetRuntime.voiceLanguage === "en"
                onTriggered: App.PetRuntime.setVoiceLanguage("en")
            }

            Platform.MenuItem {
                text: "汉语"
                checkable: true
                checked: App.PetRuntime.voiceLanguage === "zh"
                onTriggered: App.PetRuntime.setVoiceLanguage("zh")
            }
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "喂食红茶"
            enabled: App.PetRuntime.teaEnabled
            onTriggered: App.PetRuntime.requestTea()
        }

        Platform.MenuItem {
            text: App.PetRuntime.sleeping ? "唤醒" : "睡觉"
            enabled: !App.PetRuntime.sleepTransitioning
            onTriggered: App.PetRuntime.toggleSleep()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "回到待机"
            onTriggered: App.PetRuntime.returnToIdle()
        }

        Platform.MenuItem {
            text: App.PetRuntime.currentFacing === "right" ? "切到朝左" : "切到朝右"
            onTriggered: App.PetRuntime.toggleFacing()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "测试思考"
            onTriggered: App.PetRuntime.testThinking()
        }

        Platform.MenuItem {
            text: "测试说话"
            onTriggered: App.PetRuntime.testSpeaking()
        }

        Platform.MenuItem {
            text: "测试异议"
            onTriggered: App.PetRuntime.testObjecting()
        }

        Platform.MenuItem {
            text: "测试转身"
            onTriggered: App.PetRuntime.testTurn()
        }

        Platform.MenuItem {
            text: "测试走路"
            onTriggered: App.PetRuntime.testWalk()
        }

        Platform.MenuItem {
            text: "测试跑步"
            onTriggered: App.PetRuntime.testRun()
        }

        Platform.MenuItem {
            text: "测试鞠躬"
            onTriggered: App.PetRuntime.testBow()
        }

        Platform.MenuItem {
            text: "测试喝茶"
            onTriggered: App.PetRuntime.testTea()
        }

        Platform.MenuItem {
            text: "测试睡觉"
            onTriggered: App.PetRuntime.testSleep()
        }

        Platform.MenuItem {
            text: "测试徽章"
            onTriggered: App.PetRuntime.testProsecutorBadge()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "退出"
            onTriggered: Qt.quit()
        }
    }

    SoundEffect {
        id: voiceEffect

        source: App.PetRuntime.currentSoundUrl
        property int soundSerial: App.PetRuntime.soundPlaybackSerial
        volume: 0.8
    }

    // Phase 0.14 的最小 Prop 窗口：先专门承载检察官徽章。
    // 它是独立 Window，才能像旧版一样飞出桌宠本体窗口范围。
    Window {
        id: prosecutorBadgeWindow

        width: Math.max(1, App.PetRuntime.currentPropWidth)
        height: Math.max(1, App.PetRuntime.currentPropHeight)
        visible: App.PetRuntime.currentPropVisible
        color: "transparent"
        title: "Prosecutor Badge"

        flags: Qt.FramelessWindowHint
               | Qt.NoDropShadowWindowHint
               | Qt.WindowStaysOnTopHint
               | Qt.Tool

        Image {
            id: prosecutorBadgeImage

            anchors.centerIn: parent
            source: App.PetRuntime.currentPropImageUrl
            fillMode: Image.PreserveAspectFit
            width: Math.min(parent.width, 70)
            height: Math.min(parent.height, 70)
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: {
                badgeFlyAnimation.stop()
                badgeExpireTimer.stop()
                App.PetRuntime.handlePropClicked()
            }
        }
    }

    NumberAnimation {
        id: badgeFlyAnimation

        target: prosecutorBadgeWindow
        property: "x"
        duration: Math.max(1, App.PetRuntime.currentPropDurationMs)
        easing.type: Easing.OutSine
    }

    Timer {
        id: badgeExpireTimer

        interval: Math.max(1, App.PetRuntime.currentPropDurationMs)
        repeat: false
        onTriggered: App.PetRuntime.handlePropExpired()
    }

    // 和旧版一样，单击需要等一小段时间才能确认不是双击。
    // 这样双击不会先误触发一次单击分区反应。
    Timer {
        id: singleClickTimer

        interval: 300
        repeat: false
        property real clickX: 0
        property real clickY: 0

        onTriggered: App.PetRuntime.handlePrimaryClick(clickX, clickY, petWindow.width, petWindow.height)
    }

    // QML 只负责播放当前动画，具体 state/action 到资源的选择交给 PetRuntime。
    // 这样将来接入模型事件、点击交互、移动状态时，不需要反复改表现层。
    AnimatedImage {
        id: pet

        anchors.centerIn: parent
        source: App.PetRuntime.currentAnimationUrl
        cache: false
        playing: true
        fillMode: Image.PreserveAspectFit
        width: 200
        height: 200

        onCurrentFrameChanged: {
            const movementDelta = App.PetRuntime.consumeFrameMovementDelta()
            if (movementDelta.dx !== 0 || movementDelta.dy !== 0) {
                App.DesktopShell.movePetWindowBy(movementDelta.dx, movementDelta.dy)
            }

            if (App.PetRuntime.currentLoopMode === "hold"
                    && frameCount > 0
                    && currentFrame >= frameCount - 1) {
                App.PetRuntime.handleHoldAnimationReachedEnd()
                pet.playing = false
            }

            if ((App.PetRuntime.currentAutoReturnToIdle
                    || App.PetRuntime.currentLoopMode === "once")
                    && frameCount > 0
                    && currentFrame >= frameCount - 1) {
                App.PetRuntime.handleAnimationFinished()
            }

            if (App.PetRuntime.currentActionId === "idle_stand"
                    && App.PetRuntime.currentLoopMode === "loop"
                    && App.PetRuntime.currentRecipeId === ""
                    && frameCount > 0
                    && currentFrame >= frameCount - 1) {
                App.PetRuntime.handleIdleLoopFinished()
            }
        }
    }

    Connections {
        target: App.PetRuntime

        function onPlaybackSerialChanged() {
            // 同一个 action 连续触发时，source URL 可能不变。
            // playbackSerial 变化代表“这次要重新播放”，所以这里手动回到第 0 帧。
            pet.currentFrame = 0
            pet.playing = false
            pet.playing = true
        }

        function onSoundPlaybackSerialChanged() {
            if (App.PetRuntime.currentSoundUrl.toString().length === 0) {
                return
            }

            voiceEffect.stop()
            voiceEffect.play()
        }

        function onCurrentPropPlaybackSerialChanged() {
            if (!App.PetRuntime.currentPropVisible) {
                return
            }

            prosecutorBadgeWindow.x = petWindow.x + App.PetRuntime.currentPropStartOffsetX
            prosecutorBadgeWindow.y = petWindow.y + App.PetRuntime.currentPropStartOffsetY
            badgeFlyAnimation.from = prosecutorBadgeWindow.x
            badgeFlyAnimation.to = petWindow.x + App.PetRuntime.currentPropEndOffsetX
            badgeFlyAnimation.duration = Math.max(1, App.PetRuntime.currentPropDurationMs)
            badgeExpireTimer.interval = Math.max(1, App.PetRuntime.currentPropDurationMs)
            badgeFlyAnimation.restart()
            badgeExpireTimer.restart()
        }

        function onCurrentPropChanged() {
            if (App.PetRuntime.currentPropVisible) {
                return
            }

            badgeFlyAnimation.stop()
            badgeExpireTimer.stop()
        }
    }

    // Phase 0 先用最容易读懂的拖拽逻辑。
    // 后续如果要做到像旧版一样的像素级点击区域，需要交给 Pet Runtime 和 hit mask。
    MouseArea {
        id: dragArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 1

        property real pressX: 0
        property real pressY: 0
        property bool dragMoved: false

        onPressed: function(mouse) {
            if (!App.PetRuntime.pointerInteractionEnabled) {
                return
            }

            if (mouse.button === Qt.RightButton) {
                contextMenu.open()
                return
            }

            pressX = mouse.x
            pressY = mouse.y
            dragMoved = false
            App.PetRuntime.handleDragStarted(petWindow.x + mouse.x)
        }

        onPositionChanged: function(mouse) {
            if (!App.PetRuntime.pointerInteractionEnabled) {
                return
            }

            if ((mouse.buttons & Qt.LeftButton) === 0) {
                return
            }

            if (Math.abs(mouse.x - pressX) > 3 || Math.abs(mouse.y - pressY) > 3) {
                dragMoved = true
            }

            App.PetRuntime.handleDragMoved(petWindow.x + mouse.x)
            App.DesktopShell.movePetWindowBy(mouse.x - pressX, mouse.y - pressY)
        }

        onReleased: function(mouse) {
            if (!App.PetRuntime.pointerInteractionEnabled) {
                return
            }

            if (mouse.button !== Qt.LeftButton) {
                return
            }

            App.PetRuntime.handleDragEnded()

            if (dragMoved) {
                return
            }

            singleClickTimer.clickX = mouse.x
            singleClickTimer.clickY = mouse.y
            singleClickTimer.restart()
        }

        onDoubleClicked: function(mouse) {
            if (!App.PetRuntime.pointerInteractionEnabled) {
                return
            }

            if (mouse.button !== Qt.LeftButton) {
                return
            }

            singleClickTimer.stop()
            App.PetRuntime.handleDoubleClick()
        }
    }
}
