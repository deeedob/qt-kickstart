pragma ComponentBehavior: Bound
pragma Translator: CanvasPage

import QtQuick
import QtQuick.Layouts

import App.Canvas
import App.Controls

ToolButton {
    id: root

    required property CanvasStore store
    property bool aspectLinked: false
    property real aspectRatio: 1.0

    objectName: "paperSettingsButton"
    icon.source: Theme.icon("settings-2")
    text: qsTr("Canvas settings")
    onClicked: paperSettings.open()

    Popup {
        id: paperSettings
        x: root.width - width
        y: root.height + Theme.spacing / 2
        padding: Theme.spacing
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radius
            border.color: Theme.border
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacing / 2

            Label {
                text: qsTr("Canvas")
                font.weight: Font.DemiBold
            }

            GridLayout {
                id: pageProperties

                function constrainedHeight(width: int): int {
                    if (!root.aspectLinked)
                        return root.store.pageHeight;
                    return Math.max(100, Math.min(4096, Math.round(width / root.aspectRatio)));
                }

                function constrainedWidth(height: int): int {
                    if (!root.aspectLinked)
                        return root.store.pageWidth;
                    return Math.max(100, Math.min(4096, Math.round(height * root.aspectRatio)));
                }

                columns: 3
                columnSpacing: Theme.spacing / 2
                rowSpacing: Theme.spacing / 2

                Label {
                    text: qsTr("Width")
                }
                Dimension {
                    id: pageWidth
                    objectName: "pageWidth"
                    Layout.fillWidth: true
                    linkedFocus: root.aspectLinked && pageHeight.activeFocus
                    value: root.store.pageWidth
                    onValueModified: root.store.setPageSize(value, pageProperties.constrainedHeight(
                                                                value))
                }
                ToolButton {
                    objectName: "aspectLink"
                    checkable: true
                    checked: root.aspectLinked
                    icon.source: Theme.icon(root.aspectLinked ? "link-2" : "unlink-2")
                    text: root.aspectLinked ? qsTr("Unlock aspect ratio") : qsTr(
                                                  "Lock aspect ratio")
                    onClicked: {
                        root.aspectRatio = root.store.pageWidth / root.store.pageHeight;
                        root.aspectLinked = !root.aspectLinked;
                    }
                }
                Label {
                    text: qsTr("Height")
                }
                Dimension {
                    id: pageHeight
                    objectName: "pageHeight"
                    Layout.fillWidth: true
                    linkedFocus: root.aspectLinked && pageWidth.activeFocus
                    value: root.store.pageHeight
                    onValueModified: root.store.setPageSize(pageProperties.constrainedWidth(value),
                                                            value)
                }
                Item {
                    Layout.preferredWidth: Theme.controlHeight
                }

                ComboBox {
                    Layout.fillWidth: true
                    Layout.columnSpan: 3
                    model: [qsTr("Plain"), qsTr("Dots"), qsTr("Ruled")]
                    currentIndex: [Canvas.Background.Plain, Canvas.Background.Dots,
                        Canvas.Background.Ruled].indexOf(root.store.background)
                    onActivated: index => root.store.setBackground([Canvas.Background.Plain,
                                                                    Canvas.Background.Dots,
                                                                    Canvas.Background.Ruled][index])
                }

                Label {
                    visible: root.store.background !== Canvas.Background.Plain
                    text: qsTr("Spacing")
                }
                SpinBox {
                    visible: root.store.background !== Canvas.Background.Plain
                    Layout.fillWidth: true
                    from: 8
                    to: 128
                    editable: true
                    value: root.store.spacing
                    onValueModified: root.store.setPattern(value, root.store.thickness)
                }
                Item {
                    visible: root.store.background !== Canvas.Background.Plain
                    Layout.preferredWidth: Theme.controlHeight
                }

                Label {
                    visible: root.store.background !== Canvas.Background.Plain
                    text: qsTr("Thickness")
                }
                SpinBox {
                    visible: root.store.background !== Canvas.Background.Plain
                    Layout.fillWidth: true
                    from: 1
                    to: 8
                    editable: true
                    value: root.store.thickness
                    onValueModified: root.store.setPattern(root.store.spacing, value)
                }
                Item {
                    visible: root.store.background !== Canvas.Background.Plain
                    Layout.preferredWidth: Theme.controlHeight
                }
            }
        }
    }

    component Dimension: SpinBox {
        from: 100
        to: 4096
        editable: true
        suffix: "px"
        textFromValue: value => value.toString()
    }
}
