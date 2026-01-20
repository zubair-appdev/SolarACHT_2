// CreateUserPage.qml
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Item {
    id: createUserPage
    anchors.fill: stackView.view

    property string role: ""
    property Item keyboard   // keyboard reference passed from previous page
    property string type: ""

    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"

        Column {
            id: contentColumn
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            spacing: 20

            // move up when keyboard appears
            anchors.verticalCenterOffset: keyboard && keyboard.active ? -parent.height * 0.3 : 0

            Behavior on anchors.verticalCenterOffset {
                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }

            Connections{
                target: keyboard
                function onActiveChanged() {
                    statusText.visible = !keyboard.active
                }
            }

            //----------------------------------------------------------------
            // TITLE
            //----------------------------------------------------------------
            Text {
                text: type === "newMaster" ? "Create Admin Account" : "Create " + role + " Account"
                color: "white"
                font.pixelSize: 26
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            //----------------------------------------------------------------
            // NEW SECTION: MASTER KEY VALIDATION (ONLY SHOW IF type == "newMaster")
            //----------------------------------------------------------------
            TextField {
                id: masterKeyField
                visible: type === "newMaster"     // show only in master mode
                placeholderText: "Enter Master Key"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                echoMode: TextInput.Password
            }

            Button {
                id: masterValidateButton
                visible: type === "newMaster"     // show only in master mode
                text: "Validate Master Key"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {
                    if (masterKeyField.text.trim() === "") {
                        statusText.text = "⚠️ Enter Master Key first."
                        statusText.color = "orange"
                        statusText.opacity = 1
                        hideTimer.restart()
                        return
                    }

                    if (masterKeyField.text.trim() === "maytech1234") {
                        console.log("Master Key Correct!")

                        // hide master key section
                        masterKeyField.visible = false
                        masterValidateButton.visible = false

                        // show user creation fields
                        newUsername.visible = true
                        newPassword.visible = true
                        saveButton.visible = true

                    } else {
                        statusText.text = "❌ Wrong Master Key!"
                        statusText.color = "red"
                        statusText.opacity = 1
                        hideTimer.restart()
                    }
                }
            }

            //----------------------------------------------------------------
            // NORMAL USER CREATION FIELDS (HIDDEN UNTIL MASTER KEY IS VALIDATED IF TYPE IS newMaster)
            //----------------------------------------------------------------

            TextField {
                id: newUsername
                visible: type !== "newMaster"   // show by default OR after validate
                placeholderText: "Enter Username"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase
            }

            TextField {
                id: newPassword
                visible: type !== "newMaster"   // show by default OR after validate
                placeholderText: "Enter Password"
                echoMode: TextInput.Password
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Button {
                id: saveButton
                visible: type !== "newMaster"   // show by default OR after validate
                text: "Save"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {
                    if (newUsername.text.trim() === "" || newPassword.text.trim() === "") {
                        statusText.text = "⚠️ Please fill all fields."
                        statusText.color = "orange"
                        statusText.opacity = 1
                        hideTimer.restart()
                        return
                    }

                    let success = loginHandler.saveUser(newUsername.text, newPassword.text, role)

                    if (success) {
                        statusText.text = "✅ Saved successfully!"
                        statusText.color = "#00FF66"
                        statusText.opacity = 1
                        newUsername.text = ""
                        newPassword.text = ""
                    } else {
                        statusText.text = "❌ Failed to save."
                        statusText.color = "red"
                        statusText.opacity = 1
                    }

                    hideTimer.restart()
                }
            }

            //----------------------------------------------------------------
            // STATUS TEXT + TIMER
            //----------------------------------------------------------------
            Text {
                id: statusText
                text: ""
                color: "white"
                font.pixelSize: 18
                opacity: 0
                anchors.horizontalCenter: parent.horizontalCenter
                Behavior on opacity { NumberAnimation { duration: 400 } }
            }

            Timer {
                id: hideTimer
                interval: 3000
                repeat: false
                onTriggered: statusText.opacity = 0
            }

            //----------------------------------------------------------------
            // BACK BUTTON (always visible)
            //----------------------------------------------------------------
            Button {
                text: "Back"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked:

                    stackView.pop(stackView.initialItem)
            }
        }
    }
}
