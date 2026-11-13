import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import SortFilterProxyModel 0.2
import InstalledAppsModel 1.0
import Style 1.0

import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

Popup {
    id: root
    objectName: "runningAppsDialog"

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent ? parent.width - 32 : 460)
    height: Math.min(560, parent ? parent.height - 48 : 560)
    padding: 16
    modal: true
    focus: true

    onAboutToShow: {
        search.clear()
        processes.markSavedExecutables(AppSplitTunnelingController.savedExecutablePaths())
        processes.updateModel()
    }
    onOpened: search.forceActiveFocus()

    background: Rectangle {
        radius: 16
        color: AmneziaStyle.color.bgSurfaceAlt
        border.color: AmneziaStyle.color.borderSoft
    }

    InstalledAppsModel { id: processes }

    contentItem: ColumnLayout {
        spacing: 12

        SectionHeaderType {
            Layout.fillWidth: true
            headerText: qsTr("Running applications")
        }

        SearchFieldType {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Search by executable name")
        }

        ListViewType {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            model: SortFilterProxyModel {
                id: proxy
                sourceModel: processes
                filters: RegExpFilter {
                    roleName: "appName"
                    pattern: search.text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
                    caseSensitivity: Qt.CaseInsensitive
                }
                sorters: RoleSorter { roleName: "appName" }
            }

            delegate: RowLayout {
                width: list.width
                spacing: 8

                Image {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    source: "image://installedAppImage/" + appIcon
                    sourceSize: Qt.size(32, 32)
                    fillMode: Image.PreserveAspectFit
                }

                CheckBoxType {
                    Layout.fillWidth: true
                    text: appName
                    checked: isAppSaved || isAppSelected
                    enabled: !isAppSaved
                    onToggled: processes.selectedStateChanged(proxy.mapToSource(index), checked)
                }
            }

            BodyTextType {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No matching applications")
                color: AmneziaStyle.color.textSecondary
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ButtonType {
                Layout.fillWidth: true
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            ButtonType {
                Layout.fillWidth: true
                text: qsTr("Add selected")
                onClicked: {
                    const apps = processes.getSelectedAppsInfo()
                    root.close()
                    if (apps.length > 0)
                        Dialogs.applySplitTunnelingChange(function() { return AppSplitTunnelingController.addApps(apps) })
                }
            }
        }
    }
}
