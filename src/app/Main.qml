pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import App.Controls
import App.Canvas // demo
import App.Sketchbook // demo
import App.Notify

ApplicationWindow {
    id: window

    required property SketchbookViewModel sketchbook // demo
    required property CanvasStore canvas // demo
    required property NotificationManager notifications

    width: 960
    height: 680
    visible: true
    title: Application.displayName

    // demo:begin
    onClosing: function (close) {
        close.accepted = window.sketchbook.requestClose();
    }
    // demo:end

    Binding {
        target: Theme
        property: "dark"
        value: AppController.darkTheme
    }

    LoggingCategory {
        id: navigationLog
        name: "app.navigation"
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing / 2
            anchors.rightMargin: Theme.spacing / 2

            ToolButton {
                icon.source: Theme.icon("arrow-left")
                text: qsTr("Back")
                visible: stack.depth > 1
                onClicked: {
                    console.info(navigationLog, "Returning to sketchbook");
                    stack.pop();
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacing / 2
                text: window.title
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                visible: stack.depth === 1 // demo
            }

            // demo:begin
            Item {
                id: sketchTitle

                Layout.fillWidth: true
                implicitWidth: 180
                implicitHeight: Theme.controlHeight
                visible: stack.depth > 1

                property bool editing: false

                function focusEditor(): void {
                    if (sketchTitle.editing) {
                        sketchNameField.forceActiveFocus()
                        sketchNameField.selectAll()
                    }
                }

                Label {
                    anchors.fill: parent
                    visible: !sketchTitle.editing
                    text: window.sketchbook.openName
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight

                    MouseArea {
                        anchors.fill: parent
                        onDoubleClicked: {
                            sketchTitle.editing = true
                            sketchNameField.text = window.sketchbook.openName
                            Qt.callLater(sketchTitle.focusEditor)
                        }
                    }
                }

                TextField {
                    id: sketchNameField

                    anchors.fill: parent
                    visible: sketchTitle.editing
                    placeholderText: qsTr("Sketch name")
                    onEditingFinished: {
                        if (sketchTitle.editing)
                            window.sketchbook.renameOpen(text)
                        sketchTitle.editing = false
                    }
                    Keys.onEscapePressed: {
                        sketchTitle.editing = false
                        sketchNameField.focus = false
                    }
                }
            }
            // demo:end

            // demo:begin
            Label {
                visible: stack.depth > 1
                text: qsTr("%n stroke(s)", "", window.canvas.strokeCount) + " · " + qsTr("%n point(s)",
                                                                                         "", window.canvas.pointCount)
                color: Theme.mutedText
            }
            // demo:end

            ToolButton {
                icon.source: Theme.icon(AppController.darkTheme ? "sun" : "moon")
                text: AppController.darkTheme ? qsTr("Switch to light theme") : qsTr(
                                                    "Switch to dark theme")
                onClicked: AppController.darkTheme = !AppController.darkTheme
            }

            LanguageSelector {
                language: AppController.language
                onLanguageSelected: language => AppController.language = language
            }

            ToolButton {
                icon.source: Theme.icon("info")
                text: qsTr("About")
                onClicked: about.open()
            }
        }
    }

    StackView {
        id: stack

        anchors.fill: parent
        // demo:begin
        initialItem: SketchbookPage {
            viewModel: window.sketchbook
            onOpened: {
                console.info(navigationLog, "Opening canvas");
                stack.push(canvasPage);
            }
        }
        // demo:end
    }

    // demo:begin
    Component {
        id: canvasPage

        CanvasPage {
            store: window.canvas
        }
    }

    Binding {
        window.title: window.sketchbook.openName
        when: stack.depth > 1
    }
    // demo:end

    AboutDialog {
        id: about
    }

    NotificationHost {
        anchors.fill: parent
        manager: window.notifications
    }

    // demo:begin
    Connections {
        target: window.sketchbook

        function onCloseReady(): void {
        window.close();
    }
    }
        // demo:end
    }
