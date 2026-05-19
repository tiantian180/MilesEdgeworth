import QtQuick
import QtQuick.Window
import QtQml.Models
import Qt.labs.platform as Platform
import QtMultimedia
import MilesEdgeworth as App

Window {
    id: petWindow

    width: App.PetRuntime.petWindowSize
    height: App.PetRuntime.petWindowSize
    visible: true
    color: "transparent"
    title: "MilesEdgeworth v2"

    flags: Qt.FramelessWindowHint
           | Qt.NoDropShadowWindowHint

    function refreshInputMask() {
        App.DesktopShell.setPetInputMask(
            App.PetRuntime.currentAnimationUrl,
            App.PetRuntime.petImageSize,
            App.PetRuntime.petWindowSize
        )
    }

    Component.onCompleted: refreshInputMask()

    // Phase 0 先使用平台原生菜单承载最小操作入口。
    // 这样菜单的 hover、外部点击关闭、阴影和系统质感都交给 Qt/系统处理。
    Platform.Menu {
        id: contextMenu

        Platform.Menu {
            title: "调整大小"

            Platform.MenuItem {
                text: "迷你"
                checkable: true
                checked: App.PetRuntime.petSizeId === "mini"
                onTriggered: App.PetRuntime.setPetSize("mini")
            }

            Platform.MenuItem {
                text: "小"
                checkable: true
                checked: App.PetRuntime.petSizeId === "small"
                onTriggered: App.PetRuntime.setPetSize("small")
            }

            Platform.MenuItem {
                text: "中"
                checkable: true
                checked: App.PetRuntime.petSizeId === "medium"
                onTriggered: App.PetRuntime.setPetSize("medium")
            }

            Platform.MenuItem {
                text: "大"
                checkable: true
                checked: App.PetRuntime.petSizeId === "big"
                onTriggered: App.PetRuntime.setPetSize("big")
            }
        }

        Platform.Menu {
            title: "双屏选项"

            Platform.MenuItem {
                text: "单屏"
                checkable: true
                checked: App.DesktopShell.screenLayoutMode === "single"
                onTriggered: App.DesktopShell.setScreenLayoutMode("single")
            }

            Platform.MenuItem {
                text: "主屏幕在左侧"
                enabled: App.DesktopShell.screenCount > 1
                checkable: true
                checked: App.DesktopShell.screenLayoutMode === "primaryLeft"
                onTriggered: App.DesktopShell.setScreenLayoutMode("primaryLeft")
            }

            Platform.MenuItem {
                text: "主屏幕在右侧"
                enabled: App.DesktopShell.screenCount > 1
                checkable: true
                checked: App.DesktopShell.screenLayoutMode === "primaryRight"
                onTriggered: App.DesktopShell.setScreenLayoutMode("primaryRight")
            }
        }

        Platform.MenuSeparator {}

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

        Platform.Menu {
            id: skinCommandMenu

            title: "皮肤动作"
            visible: skinCommandInstantiator.count > 0
            enabled: skinCommandInstantiator.count > 0

            // 简单皮肤命令由 manifest 暴露出来，这里只动态生成菜单项。
            // 复杂玩法以后进入 Custom Interaction，不在 QML 里写具体皮肤逻辑。
            Instantiator {
                id: skinCommandInstantiator

                model: App.PetEventBridge.enabledSkinCommands
                delegate: Platform.MenuItem {
                    required property var modelData

                    text: modelData.label
                    onTriggered: App.PetEventBridge.submitMenuCommand(modelData.id)
                }

                onObjectAdded: function(index, object) {
                    skinCommandMenu.insertItem(index, object)
                }

                onObjectRemoved: function(index, object) {
                    skinCommandMenu.removeItem(object)
                }
            }
        }

        Platform.MenuItem {
            text: App.PetRuntime.sleeping ? "唤醒" : "睡觉"
            enabled: !App.PetRuntime.sleepTransitioning
            onTriggered: App.PetEventBridge.submitMenuCommand("runtime.sleep.toggle")
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: "回到待机"
            onTriggered: App.PetEventBridge.submitMenuCommand("runtime.returnToIdle")
        }

        Platform.MenuItem {
            text: App.PetRuntime.currentFacing === "right" ? "切到朝左" : "切到朝右"
            onTriggered: App.PetEventBridge.submitMenuCommand("runtime.facing.toggle")
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
        // 旧版静音是直接把 QSoundEffect 音量设为 0。
        // 这里保持同样语义：正在播放的语音也会随菜单开关立即变静音。
        volume: App.PetRuntime.audioMuted ? 0 : 0.8
    }

    // Prop 是主体外的临时视觉对象，例如徽章、掉落物或轻量特效。
    // 它使用独立 Window，才能像旧版一样飞出桌宠本体窗口范围。
    Window {
        id: propWindow

        width: Math.max(1, App.PetRuntime.currentPropWidth)
        height: Math.max(1, App.PetRuntime.currentPropHeight)
        visible: App.PetRuntime.currentPropVisible
        color: "transparent"
        title: "MilesEdgeworth Prop"

        flags: Qt.FramelessWindowHint
               | Qt.NoDropShadowWindowHint
               | Qt.WindowStaysOnTopHint
               | Qt.Tool

        Image {
            id: propImage

            anchors.centerIn: parent
            source: App.PetRuntime.currentPropImageUrl
            fillMode: Image.PreserveAspectFit
            width: Math.max(1, App.PetRuntime.currentPropVisualWidth)
            height: Math.max(1, App.PetRuntime.currentPropVisualHeight)
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onClicked: {
                propFlyAnimation.stop()
                propExpireTimer.stop()
                App.PetEventBridge.submitPropClicked()
            }
        }
    }

    NumberAnimation {
        id: propFlyAnimation

        target: propWindow
        property: "x"
        duration: Math.max(1, App.PetRuntime.currentPropDurationMs)
        easing.type: Easing.OutSine
    }

    Timer {
        id: propExpireTimer

        interval: Math.max(1, App.PetRuntime.currentPropDurationMs)
        repeat: false
        onTriggered: App.PetEventBridge.submitPropExpired()
    }

    // 和旧版一样，单击需要等一小段时间才能确认不是双击。
    // 这样双击不会先误触发一次单击分区反应。
    Timer {
        id: singleClickTimer

        interval: 300
        repeat: false
        property real clickX: 0
        property real clickY: 0

        onTriggered: App.PetEventBridge.submitPrimaryClick(clickX, clickY, petWindow.width, petWindow.height)
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
        width: App.PetRuntime.petImageSize
        height: App.PetRuntime.petImageSize

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
                App.PetEventBridge.submitIdleLoopFinished()
            }
        }
    }

    Connections {
        target: App.PetRuntime

        function onCurrentAnimationUrlChanged() {
            petWindow.refreshInputMask()
        }

        function onPetScaleChanged() {
            petWindow.refreshInputMask()
        }

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

            propWindow.x = petWindow.x + App.PetRuntime.currentPropStartX
            propWindow.y = petWindow.y + App.PetRuntime.currentPropStartY
            propFlyAnimation.from = propWindow.x
            propFlyAnimation.to = petWindow.x + App.PetRuntime.currentPropEndX
            propFlyAnimation.duration = Math.max(1, App.PetRuntime.currentPropDurationMs)
            propExpireTimer.interval = Math.max(1, App.PetRuntime.currentPropDurationMs)
            propFlyAnimation.restart()
            propExpireTimer.restart()
        }

        function onCurrentPropChanged() {
            if (App.PetRuntime.currentPropVisible) {
                return
            }

            propFlyAnimation.stop()
            propExpireTimer.stop()
        }
    }

    // Phase 0 先用最容易读懂的拖拽逻辑。
    // 后续如果要做到像旧版一样的像素级点击区域，需要交给 Pet Runtime 和 hit mask。
    MouseArea {
        id: dragArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        z: 1

        property real pressX: 0
        property real pressY: 0
        property bool dragMoved: false
        property bool doubleClickPending: false

        onPressed: function(mouse) {
            if (!App.PetRuntime.pointerInteractionEnabled) {
                return
            }

            if (mouse.button === Qt.RightButton) {
                contextMenu.open()
                return
            }

            // 旧版用 300ms clickTimer 判断单双击：第二次按下时先取消第一次
            // 单击的延迟触发，等第二次松手后只执行双击动作。
            if (singleClickTimer.running) {
                singleClickTimer.stop()
                doubleClickPending = true
            } else {
                doubleClickPending = false
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
                doubleClickPending = false
                return
            }

            if (doubleClickPending) {
                doubleClickPending = false
                App.PetEventBridge.submitDoubleClick()
                return
            }

            singleClickTimer.clickX = mouse.x
            singleClickTimer.clickY = mouse.y
            singleClickTimer.restart()
        }
    }
}
