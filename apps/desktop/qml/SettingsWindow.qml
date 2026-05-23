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

                Rectangle {
                    visible: !App.SettingsController.secretStoreAvailable
                    Layout.fillWidth: true
                    implicitHeight: warningLabel.implicitHeight + 18
                    radius: 6
                    color: "#fff3df"
                    border.color: "#d89543"

                    Label {
                        id: warningLabel
                        anchors.fill: parent
                        anchors.margins: 9
                        text: qsTr("Keychain 不可用：API Key 仅从环境变量读取，不会写入磁盘。")
                        color: "#7b4a12"
                        wrapMode: Text.WordWrap
                    }
                }

                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10
                    Layout.fillWidth: true

                    Label { text: qsTr("Base URL"); color: "#26201b" }
                    TextField {
                        id: baseUrlField
                        Layout.fillWidth: true
                        text: App.SettingsController.baseUrl
                        placeholderText: "https://api.openai.com"
                        onTextEdited: App.SettingsController.baseUrl = text
                    }

                    Label { text: qsTr("API Key"); color: "#26201b" }
                    TextField {
                        id: apiKeyField
                        Layout.fillWidth: true
                        text: App.SettingsController.apiKey
                        echoMode: TextInput.Password
                        placeholderText: "sk-..."
                        onTextEdited: App.SettingsController.apiKey = text
                    }

                    Label { text: qsTr("Model"); color: "#26201b" }
                    TextField {
                        id: modelField
                        Layout.fillWidth: true
                        text: App.SettingsController.model
                        placeholderText: "gpt-4o-mini"
                        onTextEdited: App.SettingsController.model = text
                    }

                    Label { text: qsTr("Temperature"); color: "#26201b" }
                    RowLayout {
                        Layout.fillWidth: true
                        Slider {
                            id: temperatureSlider
                            Layout.fillWidth: true
                            from: 0.0
                            to: 2.0
                            stepSize: 0.05
                            value: App.SettingsController.temperature
                            onMoved: App.SettingsController.temperature = value
                        }
                        Label {
                            text: temperatureSlider.value.toFixed(2)
                            color: "#26201b"
                            Layout.preferredWidth: 44
                        }
                    }

                    Label { text: qsTr("Max Tokens"); color: "#26201b" }
                    SpinBox {
                        id: maxTokensField
                        Layout.fillWidth: true
                        from: 1
                        to: 32768
                        stepSize: 64
                        editable: true
                        value: App.SettingsController.maxTokens
                        onValueModified: App.SettingsController.maxTokens = value
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("取消")
                onClicked: App.SettingsController.revert()
            }

            Button {
                text: qsTr("保存")
                highlighted: true
                onClicked: {
                    App.SettingsController.baseUrl = baseUrlField.text
                    App.SettingsController.apiKey = apiKeyField.text
                    App.SettingsController.model = modelField.text
                    App.SettingsController.maxTokens = maxTokensField.value
                    App.SettingsController.temperature = temperatureSlider.value
                    App.SettingsController.msPerChar = Math.round(msPerCharSlider.value)
                    App.SettingsController.personaPrompt = personaField.text
                    App.SettingsController.save()
                }
            }
        }
    }
}
