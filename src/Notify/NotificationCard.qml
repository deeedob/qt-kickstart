pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

import App.Controls
import App.Notify

FocusScope {
    id: root
    objectName: "notificationCard"

    required property Notification notification
    property Flickable viewport: null
    readonly property real viewportY: root.viewport ? root.y - root.viewport.contentY : 0
    readonly property bool presented: root.visible && (!root.viewport || (root.viewportY
                                                                          + root.height > 0
                                                                          && root.viewportY
                                                                          < root.viewport.height))
    readonly property color severityColor: badge.severityColor

    implicitWidth: card.implicitWidth
    implicitHeight: card.implicitHeight
    height: implicitHeight

    function updatePresentation(): void {
        if (root.notification)
            root.notification.setPresentationState(root.presented, hoverHandler.hovered
                                                   || root.activeFocus);
    }

    Component.onCompleted: root.updatePresentation()
    Component.onDestruction: {
        if (root.notification)
            root.notification.setPresentationState(false, false);
    }
    onActiveFocusChanged: root.updatePresentation()
    onPresentedChanged: root.updatePresentation()
    onVisibleChanged: root.updatePresentation()

    MultiEffect {
        anchors.fill: card
        source: card
        shadowEnabled: true
        shadowBlur: 0.5
        shadowVerticalOffset: 3
        shadowOpacity: 0.25
    }

    Rectangle {
        id: card
        objectName: "notificationSurface"

        implicitWidth: 320
        implicitHeight: content.implicitHeight + Theme.spacing * 1.5
        width: root.width
        height: implicitHeight
        radius: Theme.radius
        color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, hoverHandler.hovered
                       ? 0.98 : 0.90)
        border.width: 1
        border.color: Theme.border

        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: Theme.spacing * 0.75
            spacing: Theme.spacing * 0.5

            Rectangle {
                objectName: "notificationHeader"
                Layout.fillWidth: true
                implicitHeight: header.implicitHeight + Theme.spacing
                radius: Theme.radius / 2
                color: Theme.window

                RowLayout {
                    id: header
                    anchors.fill: parent
                    anchors.margins: Theme.spacing / 2
                    spacing: Theme.spacing * 0.75

                    Rectangle {
                        id: badge
                        objectName: "severityBadge"

                        readonly property int severity: root.notification
                                                        ? root.notification.severity :
                                                          Notify.Severity.Information
                        readonly property color severityColor: badge.severity
                                                               === Notify.Severity.Success
                                                               ? Theme.success : badge.severity
                                                                 === Notify.Severity.Warning
                                                                 ? Theme.warning : badge.severity
                                                                   === Notify.Severity.Error
                                                                   ? Theme.error : Theme.information

                        implicitWidth: 22
                        implicitHeight: 22
                        radius: width / 2
                        color: Qt.rgba(badge.severityColor.r, badge.severityColor.g,
                                       badge.severityColor.b, 0.18)

                        Label {
                            anchors.centerIn: parent
                            text: root.notification ? root.notification.severitySymbol : "i"
                            Accessible.name: badge.severity === Notify.Severity.Success ? qsTr(
                                                                                              "Success") :
                                                                                          badge.severity
                                                                                          === Notify.Severity.Warning
                                                                                          ? qsTr("Warning") :
                                                                                            badge.severity
                                                                                            === Notify.Severity.Error
                                                                                            ? qsTr("Error") :
                                                                                              qsTr("Information")
                            color: badge.severityColor
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.Bold
                        }
                    }

                    Label {
                        objectName: "notificationTitle"
                        Layout.fillWidth: true
                        text: root.notification ? (root.notification.title
                                                   || root.notification.message) : ""
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }

                    Item {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28

                        Canvas {
                            id: timerRing
                            anchors.fill: parent
                            onPaint: {
                                const ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                const remaining = root.notification
                                      ? root.notification.timeRemaining() : -1;
                                if (remaining < 0)
                                    return;
                                ctx.strokeStyle = root.severityColor;
                                ctx.lineWidth = 2;
                                ctx.beginPath();
                                ctx.arc(width / 2, height / 2, width / 2 - 2, -Math.PI / 2,
                                        -Math.PI / 2 + Math.PI * 2 * remaining, false);
                                ctx.stroke();
                            }
                            Timer {
                                interval: 50
                                repeat: true
                                running: root.presented && root.notification !== null
                                         && root.notification.presentation
                                         === Notify.Presentation.Passive
                                         && root.notification.timeRemaining() >= 0
                                onTriggered: timerRing.requestPaint()
                            }
                        }
                        Button {
                            objectName: "dismissNotification"
                            anchors.fill: parent
                            padding: 0
                            horizontalPadding: 0
                            verticalPadding: 0
                            flat: true
                            text: "×"
                            Accessible.name: qsTr("Dismiss notification")
                            onClicked: {
                                if (root.notification)
                                    root.notification.dismiss();
                            }
                        }
                    }
                }
            }
            Label {
                objectName: "notificationMessage"
                Layout.fillWidth: true
                visible: root.notification && root.notification.title.length > 0
                         && root.notification.message.length > 0
                text: root.notification ? root.notification.message : ""
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 13
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Flow {
                objectName: "actions"

                Layout.fillWidth: true
                visible: root.notification && root.notification.actions.length > 0
                spacing: Theme.spacing / 2

                Repeater {
                    id: actionRepeater
                    objectName: "actionRepeater"

                    model: root.notification ? root.notification.actions.length : 0

                    delegate: Button {
                        required property int index
                        readonly property bool validAction: root.notification && index >= 0
                                                            && index
                                                            < root.notification.actions.length

                        objectName: "actionButton"
                        text: validAction ? root.notification.actions[index].text : ""
                        enabled: validAction && root.notification.actions[index].enabled
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        horizontalPadding: Theme.spacing
                        verticalPadding: Theme.spacing / 3
                        implicitHeight: 28
                        implicitWidth: implicitContentWidth + horizontalPadding * 2
                        onClicked: {
                            if (validAction)
                                root.notification.choose(root.notification.actions[index].id);
                        }
                    }
                }
            }
        }
    }

    HoverHandler {
        id: hoverHandler

        onHoveredChanged: root.updatePresentation()
    }
}
