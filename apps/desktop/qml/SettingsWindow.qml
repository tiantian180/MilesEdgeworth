import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MilesEdgeworth as App

ApplicationWindow {
    id: settingsWindow

    width: 520
    height: 560
    minimumWidth: 460
    minimumHeight: 500
    visible: App.SettingsController.windowVisible
    title: qsTr("Miles 设置")
    flags: Qt.Dialog
    color: "#f7f4ef"

    onClosing: function(close) {
        close.accepted = false
        App.SettingsController.revert()
    }

    component SettingsTextField: TextField {
        id: field

        Layout.fillWidth: true
        Layout.preferredHeight: 40
        color: "#111111"
        placeholderTextColor: "#7c858d"
        selectedTextColor: "#111111"
        selectionColor: "#b9d0f2"
        leftPadding: 12
        rightPadding: 12
        background: Rectangle {
            color: "#fffdf8"
            border.color: field.activeFocus ? "#6f6258" : "#d8d1c8"
            border.width: field.activeFocus ? 2 : 1
        }
    }

    component LightButton: Button {
        id: button

        Layout.preferredHeight: 40
        contentItem: Text {
            text: button.text
            color: button.enabled ? "#26201b" : "#8f8982"
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: button.enabled ? (button.down ? "#eee7da" : "#fffdf8") : "#ebe7df"
            border.color: "#d8d1c8"
        }
    }

    component SettingsTabButton: TabButton {
        id: tabButton

        implicitHeight: 40
        contentItem: Text {
            text: tabButton.text
            color: tabButton.checked ? "#fffdf8" : "#26201b"
            font.pixelSize: 14
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: tabButton.checked ? "#2f2d2b" : "transparent"
        }
    }

    component SettingsSlider: Slider {
        id: slider

        background: Item {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 6
            width: slider.availableWidth
            height: 6

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: "#d7d9dc"
            }

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: height / 2
                color: "#2f2d2b"
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 28
            implicitHeight: 28
            radius: width / 2
            color: "#111111"
            border.color: "#d8d1c8"
            border.width: 1
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: qsTr("Miles 设置")
            color: "#26201b"
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }

        TabBar {
            id: settingsTabs

            Layout.fillWidth: true
            Layout.preferredHeight: 40
            background: Item {}

            SettingsTabButton {
                text: qsTr("模型配置")
            }

            SettingsTabButton {
                text: qsTr("角色人格")
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: settingsTabs.currentIndex

            ScrollView {
                id: modelScroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                GridLayout {
                    width: modelScroll.availableWidth
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10

                    Label { text: qsTr("当前配置"); color: "#26201b" }
                    RowLayout {
                        id: configBar

                        Layout.fillWidth: true

                        ComboBox {
                            id: configSelector
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            model: App.SettingsController.configNames
                            currentIndex: App.SettingsController.configNames.indexOf(App.SettingsController.activeModelConfig)
                            delegate: ItemDelegate {
                                width: configSelector.width
                                text: modelData
                                contentItem: Text {
                                    text: modelData
                                    color: "#26201b"
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: highlighted ? "#eee7da" : "#fffdf8"
                                }
                            }
                            contentItem: Text {
                                leftPadding: 14
                                rightPadding: 30
                                text: configSelector.displayText
                                color: "#fffdf8"
                                font.pixelSize: 14
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            indicator: Text {
                                x: configSelector.width - width - 8
                                y: 0
                                width: 24
                                height: configSelector.height
                                text: "▾"
                                color: "#fffdf8"
                                opacity: 0.85
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: "#2f2d2b"
                            }
                            popup: Popup {
                                y: configSelector.height
                                width: configSelector.width
                                implicitHeight: contentItem.implicitHeight
                                padding: 1
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: configSelector.popup.visible ? configSelector.delegateModel : null
                                    currentIndex: configSelector.highlightedIndex
                                }
                                background: Rectangle {
                                    color: "#fffdf8"
                                    border.color: "#d8d1c8"
                                }
                            }
                            onActivated: {
                                if (currentText.length > 0) {
                                    App.SettingsController.selectConfig(currentText)
                                }
                            }
                        }

                        LightButton {
                            id: addConfigButton
                            text: qsTr("新增")
                            Layout.preferredWidth: Math.max(96, (configBar.width - configBar.spacing * 2) * 0.25)
                            onClicked: App.SettingsController.addConfig()
                        }

                        LightButton {
                            id: deleteConfigButton
                            text: qsTr("删除")
                            enabled: App.SettingsController.activeModelConfig.length > 0
                            Layout.preferredWidth: Math.max(96, (configBar.width - configBar.spacing * 2) * 0.25)
                            onClicked: App.SettingsController.deleteConfig(App.SettingsController.activeModelConfig)
                        }
                    }

                    Label { text: qsTr("名称"); color: "#26201b" }
                    SettingsTextField {
                        id: configNameField
                        text: App.SettingsController.configName
                        placeholderText: "GPT 4o Mini"
                        onTextEdited: App.SettingsController.configName = text
                    }

                    Label { text: qsTr("Base URL"); color: "#26201b" }
                    SettingsTextField {
                        id: baseUrlField
                        text: App.SettingsController.baseUrl
                        placeholderText: "https://api.openai.com"
                        onTextEdited: App.SettingsController.baseUrl = text
                    }

                    Label { text: qsTr("API Key"); color: "#26201b" }
                    SettingsTextField {
                        id: apiKeyField
                        text: App.SettingsController.apiKey
                        echoMode: TextInput.Password
                        placeholderText: "sk-..."
                        onTextEdited: App.SettingsController.apiKey = text
                    }

                    Label { text: qsTr("Model"); color: "#26201b" }
                    SettingsTextField {
                        id: modelField
                        text: App.SettingsController.model
                        placeholderText: "gpt-4o-mini"
                        onTextEdited: App.SettingsController.model = text
                    }

                    Label { text: qsTr("Temperature"); color: "#26201b" }
                    SettingsTextField {
                        id: temperatureField
                        text: App.SettingsController.temperatureText
                        placeholderText: "可留空；0-2，越高回复越发散，常用 0.7"
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        onTextEdited: App.SettingsController.temperatureText = text
                    }

                    Label { text: qsTr("Max Tokens"); color: "#26201b" }
                    SettingsTextField {
                        id: maxTokensField
                        text: App.SettingsController.maxTokensText
                        placeholderText: "可留空；回复长度上限，正整数，如 2048"
                        inputMethodHints: Qt.ImhDigitsOnly
                        onTextEdited: App.SettingsController.maxTokensText = text
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Label { text: qsTr("文字节奏"); color: "#26201b" }
                    RowLayout {
                        Layout.fillWidth: true
                        SettingsSlider {
                            id: msPerCharSlider
                            Layout.fillWidth: true
                            from: 40
                            to: 200
                            stepSize: 5
                            value: App.SettingsController.msPerChar
                            onMoved: App.SettingsController.msPerChar = Math.round(value)
                        }
                        Label {
                            text: Math.round(msPerCharSlider.value) + " ms/字"
                            color: "#26201b"
                            Layout.preferredWidth: 72
                        }
                    }
                }

                Label {
                    text: qsTr("角色人格")
                    color: "#26201b"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                ScrollView {
                    id: personaScroll

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    TextArea {
                        id: personaField
                        width: personaScroll.availableWidth
                        height: Math.max(personaScroll.availableHeight, implicitHeight)
                        text: App.SettingsController.personaPrompt
                        color: "#26201b"
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        placeholderText: qsTr("当前皮肤没有 persona.md，保存后会创建。")
                        placeholderTextColor: "#82786e"
                        selectedTextColor: "#26201b"
                        selectionColor: "#b9d0f2"
                        background: Rectangle {
                            color: "#fffdf8"
                            border.color: personaField.activeFocus ? "#7b604c" : "#d8d1c8"
                            border.width: personaField.activeFocus ? 2 : 1
                        }
                        onTextEdited: App.SettingsController.personaPrompt = text
                    }
                }
            }
        }

        Label {
            visible: App.SettingsController.validationError.length > 0
            text: App.SettingsController.validationError
            color: "#b65a45"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            visible: App.SettingsController.personaError.length > 0
            text: App.SettingsController.personaError
            color: "#b65a45"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            visible: App.SettingsController.saveError.length > 0
            text: App.SettingsController.saveError
            color: "#b65a45"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            LightButton {
                id: openConfigDirectoryButton
                text: qsTr("打开配置目录")
                Layout.preferredWidth: 128
                onClicked: App.SettingsController.openConfigDirectory()
            }

            Item { Layout.fillWidth: true }

            LightButton {
                id: cancelButton
                text: qsTr("取消")
                Layout.preferredWidth: 92
                onClicked: App.SettingsController.revert()
            }

            Button {
                id: saveButton
                text: qsTr("保存")
                highlighted: true
                Layout.preferredWidth: 92
                Layout.preferredHeight: 40
                contentItem: Text {
                    text: saveButton.text
                    color: "#111111"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: saveButton.down ? "#d7dbe0" : "#eceff2"
                    border.color: "#c6ccd2"
                }
                onClicked: {
                    App.SettingsController.configName = configNameField.text
                    App.SettingsController.baseUrl = baseUrlField.text
                    App.SettingsController.apiKey = apiKeyField.text
                    App.SettingsController.model = modelField.text
                    App.SettingsController.temperatureText = temperatureField.text
                    App.SettingsController.maxTokensText = maxTokensField.text
                    App.SettingsController.msPerChar = Math.round(msPerCharSlider.value)
                    App.SettingsController.personaPrompt = personaField.text
                    App.SettingsController.save()
                }
            }
        }
    }
}
