pragma ComponentBehavior: Bound

import App.Controls
import App.Notify
import QtQuick
import QtQuick.Layouts

ApplicationWindow {
    id: root

    required property NotificationManager notifications
    property Notification lastNotification: null
    property string result: qsTr("No notification completed yet.")

    width: 720
    height: 620
    visible: true
    title: qsTr("Notification playground")

    function post(): void {
    root.lastNotification = root.notifications.post({
        title: titleField.text,
        message: messageField.text,
        severity: severity.currentIndex,
        presentation: presentation.currentIndex,
        timeoutMs: timeout.value,
        actions: includeActions.checked ? [
                                              {
                                                  id: "accept",
                                                  text: qsTr("Accept"),
                                                  enabled: true
                                              },
                                              {
                                                  id: "disabled",
                                                  text: qsTr("Unavailable"),
                                                  enabled: false
                                              },
                                              {
                                                  id: "cancel",
                                                  text: qsTr("Cancel"),
                                                  enabled: true
                                              }
                                          ] : []
    }, root, root.completed);
}

    function completed(reason: int, actionId: string): void {
    root.result = qsTr("Finished: %1, action: %2").arg(reason).arg(actionId);
}

    ScrollView {
        anchors.fill: parent
        anchors.margins: Theme.spacing * 2
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacing

            Label {
                text: qsTr("Notification playground")
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            TextField {
                id: titleField
                Layout.fillWidth: true
                placeholderText: qsTr("Title")
                text: qsTr("Could not save sketch")
            }
            TextField {
                id: messageField
                Layout.fillWidth: true
                placeholderText: qsTr("Message")
                text: qsTr("Your changes are still available. Try again?")
            }
            RowLayout {
                Label {
                    text: qsTr("Severity")
                }
                ComboBox {
                    id: severity
                    model: [qsTr("Information"), qsTr("Success"), qsTr("Warning"), qsTr("Error")]
                }
                Label {
                    text: qsTr("Presentation")
                }
                ComboBox {
                    id: presentation
                    model: [qsTr("Passive"), qsTr("Persistent"), qsTr("Decision")]
                }
            }
            RowLayout {
                Label {
                    text: qsTr("Timeout (ms)")
                }
                SpinBox {
                    id: timeout
                    from: 0
                    to: 60000
                    stepSize: 1000
                    value: 5000
                }
                Switch {
                    id: includeActions
                    text: qsTr("Add actions")
                    checked: true
                }
                Switch {
                    id: customViews
                    objectName: "customViews"
                    text: qsTr("Custom views")
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.spacing / 2
                Button {
                    text: qsTr("Post")
                    onClicked: root.post()
                }
                Button {
                    text: qsTr("Post short message")
                    onClicked: root.lastNotification = root.notifications.postMessage(
                                   messageField.text)
                }
                Button {
                    text: qsTr("Post three")
                    onClicked: {
                        root.post();
                        root.post();
                        root.post();
                    }
                }
                Button {
                    text: qsTr("Change last presentation")
                    enabled: root.lastNotification !== null && !root.lastNotification.isFinished
                    onClicked: root.lastNotification.setPresentation(presentation.currentIndex)
                }
                Button {
                    text: qsTr("Dismiss passive")
                    onClicked: root.notifications.dismissPassive()
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.result
                wrapMode: Text.Wrap
            }
        }
    }

    NotificationHost {
        id: host
        anchors.fill: parent
        manager: root.notifications
    }

    Binding {
        target: host
        property: "notificationDelegate"
        value: customNotification
        when: customViews.checked
    }

    Binding {
        target: host
        property: "decisionDelegate"
        value: customDecision
        when: customViews.checked
    }

    Component {
        id: customNotification
        NotificationCard {
            viewport: ListView.view as Flickable
            width: viewport ? viewport.width - Theme.spacing : implicitWidth
            x: Theme.spacing / 2
            opacity: 0.8
        }
    }
    Component {
        id: customDecision

        FocusScope {
            id: customView
            objectName: "customDecisionView"

            property Notification notification: root.notifications.currentDecision
            implicitWidth: 320
            implicitHeight: customColumn.implicitHeight + Theme.spacing * 2

            function focusAction(): void {
            for (let i = 0; i < customActions.count; ++i) {
                const action = customActions.itemAt(i);
                if (action && action.enabled) {
                    action.forceActiveFocus();
                    return;
                }
                }
                    if (closeButton.visible)
                    closeButton.forceActiveFocus();
                    else
                    customView.forceActiveFocus();
                }

                    onActiveFocusChanged: {
                        if (customView.activeFocus)
                            Qt.callLater(customView.focusAction);
                    }
                    onNotificationChanged: {
                        if (customView.activeFocus)
                            Qt.callLater(customView.focusAction);
                    }
                    Keys.onEscapePressed: {
                        if (customView.notification)
                            customView.notification.dismiss();
                    }

                    ColumnLayout {
                        id: customColumn
                        objectName: "customDecisionContent"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.spacing
                        spacing: Theme.spacing / 2

                        Label {
                            Layout.fillWidth: true
                            text: customView.notification ? customView.notification.title : ""
                            font.weight: Font.Bold
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: customView.notification ? customView.notification.message : ""
                            wrapMode: Text.Wrap
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.spacing / 2

                            Repeater {
                                id: customActions
                                objectName: "customDecisionActions"

                                model: customView.notification
                                       ? customView.notification.actions.length : 0

                                delegate: Button {
                                    required property int index
                                    readonly property bool validAction: customView.notification
                                                                        && index >= 0 && index
                                                                        < customView.notification.actions.length

                                    text: validAction ? customView.notification.actions[index].text :
                                                        ""
                                    enabled: validAction
                                             && customView.notification.actions[index].enabled
                                    font.pixelSize: 12
                                    horizontalPadding: Theme.spacing
                                    verticalPadding: Theme.spacing / 3
                                    implicitHeight: 28
                                    implicitWidth: implicitContentWidth + horizontalPadding * 2
                                    onClicked: {
                                        if (validAction)
                                            customView.notification.choose(
                                                        customView.notification.actions[index].id);
                                    }
                                }
                            }

                            Button {
                                id: closeButton

                                visible: customActions.count === 0
                                text: qsTr("Close")
                                onClicked: {
                                    if (customView.notification)
                                        customView.notification.dismiss();
                                }
                            }
                        }
                    }
                }
            }
        }
