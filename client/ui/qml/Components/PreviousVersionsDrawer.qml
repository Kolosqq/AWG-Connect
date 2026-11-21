import QtQuick
import QtQuick.Controls

import "../Controls2"

SheetType {
    id: root

    signal versionSelected(string version)

    expandedHeight: Math.min(windowHeight * 0.9, header.implicitHeight + list.contentHeight + 32
                             + PageController.safeAreaBottomMargin)

    SectionHeaderType {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16

        headerText: qsTr("Choose a previous version")
    }

    ListViewType {
        id: list

        anchors.top: header.bottom
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        model: UpdateController.previousVersions

        footer: Item {
            width: list.width
            height: PageController.safeAreaBottomMargin
        }

        delegate: ListItemType {
            required property string modelData

            width: list.width
            text: modelData
            trailingIconSource: "qrc:/images/controls/chevron-right.svg"

            onClicked: {
                root.close()
                root.versionSelected(modelData)
            }
        }
    }
}
