pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window

import App.Controls
import App.Notify

Item {
    id: root

    required property NotificationManager manager
    property alias notificationDelegate: cardList.delegate
    property alias decisionDelegate: decisionDialog.decisionDelegate
    readonly property bool hostActive: Window.window ? Window.window.active : true

    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    Component.onCompleted: {
        if (root.manager.currentDecision)
            decisionDialog.open();
    }

    Component.onDestruction: root.manager.shutdown()

    Binding {
        target: root.manager
        property: "hostActive"
        value: root.hostActive
    }

    Timer {
        id: decisionCloseTimer

        interval: 0
        onTriggered: {
            if (!root.manager.currentDecision)
                decisionDialog.close();
        }
    }

    ListView {
        id: cardList
        objectName: "cardList"

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Theme.spacing * 3
        width: Math.min(332, Math.max(0, root.width - Theme.spacing * 2))
        height: Math.min(contentHeight, Math.min(216, root.height * 0.4))
        spacing: Theme.spacing
        model: root.manager
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            target: null
            onWheel: event => {
                const delta = event.pixelDelta.y || event.angleDelta.y / 2;

                cardList.contentY = Math.max(0, Math.min(cardList.contentHeight - cardList.height,
                                                         cardList.contentY - delta));
                event.accepted = true;
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: defaultNotification
    }

    Button {
        id: countButton
        objectName: "notificationCount"

        anchors.right: cardList.right
        anchors.bottom: cardList.top
        anchors.bottomMargin: Theme.spacing / 2
        visible: cardList.contentHeight > cardList.height + 1
        width: 26
        height: 26
        padding: 0
        horizontalPadding: 0
        verticalPadding: 0
        background: Rectangle {
            radius: width / 2
            color: countButton.hovered ? Theme.hover : Theme.surface
            border.color: Theme.border
        }
        text: hovered || visualFocus ? "×" : root.manager.count.toString()
        Accessible.name: qsTr("Dismiss passive notifications")
        onClicked: root.manager.dismissPassive()
    }

    NotificationDialog {
        id: decisionDialog

        notification: root.manager.currentDecision
    }

    Component {
        id: defaultNotification

        NotificationCard {
            viewport: cardList
            width: Math.max(0, cardList.width - Theme.spacing)
            x: Theme.spacing / 2
        }
    }

    Connections {
        target: root.manager

        function onRowsInserted(): void {
        Qt.callLater(cardList.positionViewAtBeginning);
    }

        function onCurrentDecisionChanged(): void {
                                                 if (root.manager.currentDecision) {
                                                     decisionCloseTimer.stop();
                                                     if (!decisionDialog.visible)
                                                     decisionDialog.open();
                                                     return;
                                                 }
                                                 decisionCloseTimer.restart();
                                             }
    }
}
