pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window

import App.Controls
import App.Notify

Dialog {
    id: root

    required property Notification notification
    property Component decisionDelegate: defaultDecision
    objectName: "decisionDialog"

    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: Overlay.overlay
    width: Math.min(380, Math.max(0, parent ? parent.width - Theme.spacing * 2 : 380))
    height: Math.min(decisionLoader.implicitHeight, Math.max(0, parent ? parent.height - Theme.spacing
                                                                         * 2 : decisionLoader.implicitHeight))
    padding: 0

    function revealFocus(): void {
        const action = decisionScroll.focusedItem;
        if (!action || !root.visible)
            return;
        let ancestor = action;
        while (ancestor && ancestor !== decisionLoader.item)
            ancestor = ancestor.parent;
        if (!ancestor)
            return;
        const y = action.mapToItem(decisionLoader.item, 0, 0).y;
        decisionScroll.flickable.contentY = Math.max(0, Math.min(y, y + action.height
                                                                 - decisionScroll.availableHeight));
    }

    function focusContent(): void {
        if (decisionLoader.item)
            (decisionLoader.item as Item).forceActiveFocus();
    }

    onNotificationChanged: {
        if (root.visible && root.notification)
            Qt.callLater(root.focusContent);
    }

    onOpened: {
        root.focusContent();
        Qt.callLater(root.revealFocus);
    }
    onRejected: {
        if (root.notification)
            root.notification.dismiss();
    }

    Component {
        id: defaultDecision

        FocusScope {
            id: decisionContent
            objectName: "notificationDecision"

            property Notification notification: root.notification
            implicitWidth: 360
            implicitHeight: content.implicitHeight + Theme.spacing * 1.5

            function focusAction(): void {
                for (let i = 0; i < actions.count; ++i) {
                    const button = actions.itemAt(i);
                    if (button && button.enabled) {
                        button.forceActiveFocus();
                        return;
                    }
                }
                decisionContent.forceActiveFocus();
            }

            Keys.onEscapePressed: {
                if (decisionContent.notification)
                    decisionContent.notification.dismiss();
            }
            onActiveFocusChanged: {
                if (decisionContent.activeFocus)
                    Qt.callLater(decisionContent.focusAction);
            }
            onNotificationChanged: {
                if (decisionContent.activeFocus)
                    Qt.callLater(decisionContent.focusAction);
            }

            ColumnLayout {
                id: content
                width: decisionContent.width - Theme.spacing * 1.5
                x: Theme.spacing * 0.75
                y: Theme.spacing * 0.75
                spacing: Theme.spacing * 0.5

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing / 2

                    Rectangle {
                        id: badge
                        objectName: "severityBadge"

                        readonly property int severity: decisionContent.notification
                                                        ? decisionContent.notification.severity :
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
                            text: decisionContent.notification
                                  ? decisionContent.notification.severitySymbol : "i"
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
                        Layout.fillWidth: true
                        text: decisionContent.notification ? decisionContent.notification.title : ""
                        visible: text.length > 0
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: Theme.text
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: decisionContent.notification ? decisionContent.notification.message : ""
                    visible: text.length > 0
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    color: Theme.text
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
                Flow {
                    Layout.fillWidth: true
                    visible: decisionContent.notification
                             && decisionContent.notification.actions.length > 0
                    spacing: Theme.spacing / 2

                    Repeater {
                        id: actions
                        model: decisionContent.notification
                               ? decisionContent.notification.actions.length : 0

                        delegate: Button {
                            required property int index
                            readonly property bool valid: decisionContent.notification && index
                                                          < decisionContent.notification.actions.length
                            text: valid ? decisionContent.notification.actions[index].text : ""
                            enabled: valid && decisionContent.notification.actions[index].enabled
                            font.pixelSize: 12
                            horizontalPadding: Theme.spacing
                            verticalPadding: Theme.spacing / 3
                            implicitHeight: 28
                            implicitWidth: implicitContentWidth + horizontalPadding * 2
                            onClicked: {
                                if (valid)
                                    decisionContent.notification.choose(
                                                decisionContent.notification.actions[index].id);
                            }
                        }
                    }
                }
            }
        }
    }

    background: Item {
        MultiEffect {
            anchors.fill: decisionSurface
            source: decisionSurface
            shadowEnabled: true
            shadowBlur: 0.5
            shadowVerticalOffset: 3
            shadowOpacity: 0.25
        }
        Rectangle {
            id: decisionSurface
            anchors.fill: parent
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
        }
    }
    contentItem: ScrollView {
        id: decisionScroll

        readonly property Flickable flickable: decisionScroll.contentItem as Flickable
        readonly property Item focusedItem: Window.window ? Window.window.activeFocusItem : null

        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        onFocusedItemChanged: {
            if (root.visible)
                Qt.callLater(root.revealFocus);
        }

        Loader {
            id: decisionLoader

            width: parent ? parent.width : 0
            sourceComponent: root.decisionDelegate
            onLoaded: {
                if (root.visible)
                    Qt.callLater(root.focusContent);
            }
            onImplicitHeightChanged: Qt.callLater(root.revealFocus)
        }
    }
}
