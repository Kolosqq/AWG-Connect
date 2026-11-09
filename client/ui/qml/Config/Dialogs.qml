pragma Singleton

import QtQuick

QtObject {
    signal questionRequested(string headerText, string descriptionText, string yesButtonText, string noButtonText,
                             var yesButtonFunction, var noButtonFunction)

    function applySplitTunnelingChange(save, title, description, restore) {
        if (Qt.platform.os === "windows" && ConnectionController.isConnectionInProgress)
            return

        const reconnect = Qt.platform.os === "windows" && ConnectionController.isConnected
        function apply() {
            if (Qt.platform.os === "windows" && ConnectionController.isConnectionInProgress)
                return
            if (save() !== false && reconnect && !ConnectionController.applySplitTunnelingChanges() && restore)
                restore()
        }
        if (reconnect) {
            askQuestion(title || qsTr("Save split tunneling changes?"),
                        (description ? description + "\n\n" : "") + qsTr("The VPN will reconnect to apply the changes."),
                        qsTr("Save and reconnect"), qsTr("Cancel"), apply, function() {})
        } else if (title) {
            askQuestion(title, description || "", qsTr("Continue"), qsTr("Cancel"), apply, function() {})
        } else {
            apply()
        }
    }

    function askQuestion(headerText, descriptionText, yesButtonText, noButtonText, yesButtonFunction, noButtonFunction) {
        questionRequested(headerText, descriptionText, yesButtonText, noButtonText, yesButtonFunction, noButtonFunction)
    }
}
