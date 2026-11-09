import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QtCore

import SortFilterProxyModel 0.2

import PageEnum 1.0
import ContainerProps 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

PageType {
    id: root

    property bool pageEnabled: Qt.platform.os === "windows"
                               && !ConnectionController.isConnectionInProgress

    Component.onCompleted: {
        if (Qt.platform.os === "windows")
            return
        if (ConnectionController.isConnected) {
            PageController.showNotificationMessage(qsTr("Cannot change split tunneling settings during active connection"))
            root.pageEnabled = false
        } else {
            root.pageEnabled = true
        }
    }

    readonly property var routeModes: [
        { type: 1, segmentName: qsTr("Via VPN"), name: qsTr("Only the apps from the list should have access via VPN") },
        { type: 2, segmentName: qsTr("Directly"), name: qsTr("Apps from the list should not have access via VPN") }
    ]

    readonly property int routeModeIndex: AppSplitTunnelingController.routeMode === 2 ? 1 : 0

    ListViewType {
        id: listView

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: addAppPanel.top

        model: SortFilterProxyModel {
            id: proxyAppSplitTunnelingModel
            sourceModel: AppSplitTunnelingModel
            filters: RegExpFilter {
                roleName: "appPath"
                pattern: ".*" + searchField.textField.text + ".*"
                caseSensitivity: Qt.CaseInsensitive
            }
            sorters: [
                RoleSorter { roleName: "appPath"; sortOrder: Qt.AscendingOrder }
            ]
        }

        header: ColumnLayout {
            width: listView.width

            z: 2

            spacing: 0

            onHeightChanged: {
                if (!listView.moving) {
                    Qt.callLater(listView.positionViewAtBeginning)
                }
            }

            NavigationBarType {
                id: navigationBar

                Layout.fillWidth: true

                title: qsTr("App split tunneling")
            }

            ListItemType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: root.pageEnabled

                text: qsTr("App split tunneling")

                onClicked: {
                    splitTunnelingSwitch.toggle()
                    splitTunnelingSwitch.toggled()
                }

                SwitchType {
                    id: splitTunnelingSwitch

                    focusPolicy: Qt.NoFocus

                    checked: AppSplitTunnelingController.isSplitTunnelingEnabled

                    onToggled: {
                        const requested = checked
                        checked = Qt.binding(function() { return AppSplitTunnelingController.isSplitTunnelingEnabled })
                        Dialogs.applySplitTunnelingChange(function() { AppSplitTunnelingController.toggleSplitTunneling(requested) },
                                                           "", "", function() { AppSplitTunnelingController.toggleSplitTunneling(!requested) })
                    }
                }
            }

            SegmentedControlType {
                id: modeSelector
                objectName: "appRouteModeSelector"

                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: (Qt.platform.os === "android" || Qt.platform.os === "windows") && root.pageEnabled

                model: root.routeModes.map(function(mode) { return mode.segmentName })
                currentIndex: root.routeModeIndex

                onActivated: function(index) {
                    if (AppSplitTunnelingController.routeMode !== root.routeModes[index].type) {
                        const requested = root.routeModes[index].type
                        modeSelector.currentIndex = Qt.binding(function() { return root.routeModeIndex })
                        const previous = AppSplitTunnelingController.routeMode
                        Dialogs.applySplitTunnelingChange(function() { AppSplitTunnelingController.routeMode = requested },
                                                           "", "", function() { AppSplitTunnelingController.routeMode = previous })
                    }
                }

                Connections {
                    target: AppSplitTunnelingController

                    function onRouteModeChanged() {
                        modeSelector.currentIndex = root.routeModeIndex
                    }
                }
            }

            BodyTextType {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 24

                text: root.routeModes[modeSelector.currentIndex].name
                color: AmneziaStyle.color.textSecondary
            }

            NotificationType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 24

                text: qsTr("Via VPN mode supports AmneziaWG and WireGuard with site split tunneling turned off. Kill Switch protects selected apps only; strict mode also blocks them after manual disconnect. System DNS stays direct. An empty list sends all apps directly.")

                visible: (Qt.platform.os === "windows") && root.pageEnabled && root.routeModeIndex === 0
            }
        }

        delegate: Item {
            width: listView.width
            implicitHeight: appItem.implicitHeight
            enabled: root.pageEnabled

            ListItemType {
                id: appItem

                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                text: appPath
                trailingIconSource: "qrc:/images/controls/trash.svg"

                onClicked: {
                    var headerText = qsTr("Remove ") + appPath + "?"
                    const sourceIndex = proxyAppSplitTunnelingModel.mapToSource(index)
                    Dialogs.applySplitTunnelingChange(function() {
                        return AppSplitTunnelingController.removeApp(sourceIndex)
                    }, headerText)
                }
            }
        }
    }

    Rectangle {
        id: addAppPanel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: searchField.textField.activeFocus ? 0 : PageController.imeHeight

        implicitHeight: searchField.implicitHeight + 32 + PageController.safeAreaBottomMargin

        color: AmneziaStyle.color.bgBase

        TextFieldType {
            id: searchField

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 24
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            enabled: root.pageEnabled

            textField.placeholderText: qsTr("application name")
            buttonImageSource: "qrc:/images/controls/plus.svg"
            buttonJoined: true

            rightButtonClickedOnEnter: true

            clickedFunc: function() {
                searchField.focus = false
                PageController.showBusyIndicator(true)

                if (Qt.platform.os === "windows") {
                    appSelectionDrawer.open()
                } else if (Qt.platform.os === "android"){
                    installedAppDrawer.open()
                }

                PageController.showBusyIndicator(false)
            }
        }
    }

    InstalledAppsDrawer {
        id: installedAppDrawer
    }

    AppSelectionDrawer {
        id: appSelectionDrawer
        onRunningAppsRequested: installedAppDrawer.open()
    }
}
