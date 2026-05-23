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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        ScrollView {
            id: settingsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: settingsScroll.availableWidth
                spacing: 12

                Label {
                    text: qsTr("Provider 配置")
                    color: "#26201b"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Label { text: qsTr("当前配置"); color: "#26201b" }
                    RowLayout {
                        Layout.fillWidth: true

                        ComboBox {
                            id: configSelector
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            model: App.SettingsController.configNames
                            currentIndex: App.SettingsController.configNames.indexOf(App.SettingsController.activeModelConfig)
                            contentItem: Text {
                                leftPadding: 14
                                rightPadding: 38
                                text: configSelector.displayText
                                color: "#fffdf8"
                                font.pixelSize: 14
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            indicator: Text {
                                x: configSelector.width - width - 12
                                y: (configSelector.height - height) / 2
                                text: "⌄"
                                color: "#d8d1c8"
                                font.pixelSize: 22
                            }
                            background: Rectangle {
                                color: "#2f2d2b"
                                border.color: "#2f2d2b"
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

                        Button {
                            id: addConfigButton
                            text: qsTr("新增")
                            Layout.preferredWidth: Math.max(96, (parent.width - parent.spacing * 2) * 0.25)
                            Layout.preferredHeight: 40
                            contentItem: Text {
                                text: addConfigButton.text
                                color: "#26201b"
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: addConfigButton.down ? "#eee7da" : "#fffdf8"
                                border.color: "#d8d1c8"
                            }
                            onClicked: App.SettingsController.addConfig()
                        }

                        Button {
                            id: deleteConfigButton
                            text: qsTr("删除")
                            enabled: App.SettingsController.activeModelConfig.length > 0
                            Layout.preferredWidth: Math.max(96, (parent.width - parent.spacing * 2) * 0.25)
                            Layout.preferredHeight: 40
                            contentItem: Text {
                                text: deleteConfigButton.text
                                color: deleteConfigButton.enabled ? "#26201b" : "#8f8982"
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: deleteConfigButton.enabled
                                        ? (deleteConfigButton.down ? "#eee7da" : "#fffdf8")
                                        : "#ebe7df"
                                border.color: "#d8d1c8"
                            }
                            onClicked: App.SettingsController.deleteConfig(App.SettingsController.activeModelConfig)
                        }
                    }

                    Label { text: qsTr("名称"); color: "#26201b" }
                    TextField {
                        id: configNameField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: App.SettingsController.configName
                        placeholderText: "GPT 4o Mini"
                        color: "#111111"
                        leftPadding: 12
                        rightPadding: 12
                        placeholderTextColor: "#7c858d"
                        selectedTextColor: "#111111"
                        selectionColor: "#b9d0f2"
                        background: Rectangle {
                            color: "#fffdf8"
                            border.color: configNameField.activeFocus ? "#6f6258" : "#d8d1c8"
                            border.width: configNameField.activeFocus ? 2 : 1
                        }
                        onTextEdited: App.SettingsController.configName = text
                    }

                    Label { text: qsTr("Base URL"); color: "#26201b" }
                    TextField {
                        id: baseUrlField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: App.SettingsController.baseUrl
                        placeholderText: "https://api.openai.com"
                        color: "#111111"
                        leftPadding: 12
                        rightPadding: 12
                        placeholderTextColor: "#7c858d"
                        selectedTextColor: "#111111"
                        selectionColor: "#b9d0f2"
                        background: Rectangle {
                            color: "#fffdf8"
                            border.color: baseUrlField.activeFocus ? "#6f6258" : "#d8d1c8"
                            border.width: baseUrlField.activeFocus ? 2 : 1
                        }
                        onTextEdited: App.SettingsController.baseUrl = text
                    }

                    Label { text: qsTr("API Key"); color: "#26201b" }
                    TextField {
                        id: apiKeyField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: App.SettingsController.apiKey
                        echoMode: TextInput.Password
                        placeholderText: "sk-..."
                        color: "#111111"
                        leftPadding: 12
                        rightPadding: 12
                        placeholderTextColor: "#7c858d"
                        selectedTextColor: "#111111"
                        selectionColor: "#b9d0f2"
                        background: Rectangle {
                            color: "#fffdf8"
                            border.color: apiKeyField.activeFocus ? "#6f6258" : "#d8d1c8"
                            border.width: apiKeyField.activeFocus ? 2 : 1
                        }
                        onTextEdited: App.SettingsController.apiKey = text
                    }

                    Label { text: qsTr("Model"); color: "#26201b" }
                    TextField {
                        id: modelField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: App.SettingsController.model
                        placeholderText: "gpt-4o-mini"
                        color: "#111111"
                        leftPadding: 12
                        rightPadding: 12
                        placeholderTextColor: "#7c858d"
                        selectedTextColor: "#111111"
                        selectionColor: "#b9d0f2"
                        background: Rectangle {
                            color: "#fffdf8"
                            border.color: modelField.activeFocus ? "#6f6258" : "#d8d1c8"
                            border.width: modelField.activeFocus ? 2 : 1
                        }
                        onTextEdited: App.SettingsController.model = text
                    }

                    Label { text: qsTr("Temperature"); color: "#26201b" }
                    TextField {
                        id: temperatureField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: App.SettingsController.temperatureText
                        placeholderText: "可留空；0-2，越高回复越发散，常用 0.7"
                        color: "#111111"
                        leftPadding: 12
                        rightPadding: 12
                        placeholderTextColor: "#7c858d"
                        selectedTextColor: "#111111"
                        selectionColor: "#b9d0f2"
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        background: Rectangle {
                            color: "#fffdf8"
                            border.color: temperatureField.activeFocus ? "#6f6258" : "#d8d1c8"
                            border.width: temperatureField.activeFocus ? 2 : 1
                        }
                        onTextEdited: App.SettingsController.temperatureText = text
                    }

                    Label { text: qsTr("Max Tokens"); color: "#26201b" }
                    TextField {
                        id: maxTokensField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        text: App.SettingsController.maxTokensText
                        placeholderText: "可留空；回复长度上限，正整数，如 2048"
                        color: "#111111"
                        leftPadding: 12
                        rightPadding: 12
                        placeholderTextColor: "#7c858d"
                        selectedTextColor: "#111111"
                        selectionColor: "#b9d0f2"
                        inputMethodHints: Qt.ImhDigitsOnly
                        background: Rectangle {
                            color: "#fffdf8"
                            border.color: maxTokensField.activeFocus ? "#6f6258" : "#d8d1c8"
                            border.width: maxTokensField.activeFocus ? 2 : 1
                        }
                        onTextEdited: App.SettingsController.maxTokensText = text
                    }

                    Label { text: qsTr("文字节奏"); color: "#26201b" }
                    RowLayout {
                        Layout.fillWidth: true
                        Slider {
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
                    visible: App.SettingsController.validationError.length > 0
                    text: App.SettingsController.validationError
                    color: "#b65a45"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("角色人格")
                    color: "#26201b"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                TextArea {
                    id: personaField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    text: App.SettingsController.personaPrompt
                    color: "#26201b"
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    placeholderText: qsTr("当前皮肤没有 persona.md，保存后会创建。")
                    placeholderTextColor: "#82786e"
                    selectedTextColor: "#26201b"
                    selectionColor: "#b9d0f2"
                    background: Rectangle {
                        radius: 6
                        color: "#fffdf8"
                        border.color: personaField.activeFocus ? "#7b604c" : "#d8d1c8"
                        border.width: personaField.activeFocus ? 2 : 1
                    }
                    onTextEdited: App.SettingsController.personaPrompt = text
                }

                Label {
                    visible: App.SettingsController.personaError.length > 0
                    text: App.SettingsController.personaError
                    color: "#b65a45"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

            }
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

            Item { Layout.fillWidth: true }

            Button {
                id: cancelButton
                text: qsTr("取消")
                Layout.preferredWidth: 92
                Layout.preferredHeight: 40
                contentItem: Text {
                    text: cancelButton.text
                    color: "#26201b"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: cancelButton.down ? "#eee7da" : "#fffdf8"
                    border.color: "#d8d1c8"
                }
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
