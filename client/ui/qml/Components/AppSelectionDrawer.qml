import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import "../Controls2"
import "../Config"

SheetType {
    id: root
    objectName: "appSelectionDrawer"

    signal runningAppsRequested()

    expandedHeight: choices.implicitHeight + 48 + PageController.safeAreaBottomMargin

    ColumnLayout {
        id: choices
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16

        ListItemType {
            Layout.fillWidth: true
            text: qsTr("Choose executable")
            onClicked: {
                root.close()
                var path = SystemController.getFileName(qsTr("Open executable file"), qsTr("Executable files (*.exe)"))
                if (path !== "") {
                    Dialogs.applySplitTunnelingChange(function() { return AppSplitTunnelingController.addApp(path) })
                }
            }
        }
        ListItemType {
            Layout.fillWidth: true
            text: qsTr("Choose folder")
            onClicked: folderDialog.open()
        }
        ListItemType {
            Layout.fillWidth: true
            text: qsTr("Choose running application")
            onClicked: {
                root.close()
                root.runningAppsRequested()
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a folder including its subfolders")
        onAccepted: {
            const folder = selectedFolder
            Dialogs.applySplitTunnelingChange(function() { return AppSplitTunnelingController.addFolder(folder) })
            root.close()
        }
    }
}
