pragma ComponentBehavior: Bound

import App.Controls
import App.Notify as Notify
import QtQuick

Rectangle {
    id: root

    required property int severity
    readonly property color severityColor: {
        switch (root.severity) {
        case Notify.Severity.Success:
            return Theme.success;
        case Notify.Severity.Warning:
            return Theme.warning;
        case Notify.Severity.Error:
            return Theme.error;
        default:
            return Theme.information;
        }
    }

    implicitWidth: 22
    implicitHeight: 22
    radius: width / 2
    color: Qt.rgba(root.severityColor.r, root.severityColor.g, root.severityColor.b, 0.18)

    Label {
        anchors.centerIn: parent
        text: {
            switch (root.severity) {
            case Notify.Severity.Success:
                return "✓";
            case Notify.Severity.Warning:
                return "!";
            case Notify.Severity.Error:
                return "×";
            default:
                return "i";
            }
        }
        Accessible.name: {
            switch (root.severity) {
            case Notify.Severity.Success:
                return qsTr("Success");
            case Notify.Severity.Warning:
                return qsTr("Warning");
            case Notify.Severity.Error:
                return qsTr("Error");
            default:
                return qsTr("Information");
            }
        }
        color: root.severityColor
        font.family: Theme.fontFamily
        font.pixelSize: 13
        font.weight: Font.Bold
    }
}
