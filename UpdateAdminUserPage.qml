import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Item {
    id: updateAdminUserPage
    anchors.fill: stackView.view

    property string role: ""
    property Item keyboard   // keyboard reference passed from previous page

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

            // Listen to keyboard active changes (safe and simple)
            Connections {
                target: keyboard
                function onActiveChanged() {
                    statusText.visible = !keyboard.active   // hide when active, show when inactive
                }
            }


            Behavior on anchors.verticalCenterOffset {
                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }

            Text {
                id: titleText
                text: "Recover " + role + " Account"
                color: "white"
                font.pixelSize: 26
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            TextField {
                id: username
                placeholderText: "Username"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase
            }

            Row {
                spacing: 20
                anchors.horizontalCenter: parent.horizontalCenter

                TextField {
                    id: newPassword
                    placeholderText: "New Password"
                    echoMode: TextInput.Password
                }

                TextField {
                    id: confirmPassword
                    placeholderText: "Confirm Password"
                    echoMode: TextInput.Password
                }
            }


            Button {
                text: "Update"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {
                    let u = username.text.trim()
                    let p1 = newPassword.text.trim()
                    let p2 = confirmPassword.text.trim()

                    // ---- validation ----
                    if (u === "" || p1 === "" || p2 === "") {
                        statusText.text = "⚠️ Please fill all fields."
                        statusText.color = "orange"
                        statusText.opacity = 1
                        hideTimer.restart()
                        return
                    }

                    if (p1 !== p2) {
                        statusText.text = "⚠️ Passwords do not match."
                        statusText.color = "red"
                        statusText.opacity = 1
                        hideTimer.restart()
                        return
                    }

                    // ---- backend call ----
                    let ok = loginHandler.updatePassword(u, p1, role)

                    if (ok) {
                        statusText.text = "✅ Password updated successfully!"
                        statusText.color = "#00FF66"
                        statusText.opacity = 1

                        username.text = ""
                        newPassword.text = ""
                        confirmPassword.text = ""

                    } else {
                        statusText.text = "❌ Username not found in " + role
                        statusText.color = "red"
                        statusText.opacity = 1
                    }

                    hideTimer.restart()
                }
            }


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
                running: false
                repeat: false
                onTriggered: statusText.opacity = 0
            }

            Button {
                text: "Home"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {
                    stackView.pop(stackView.initialItem);
                }
            }


        }
    }
}
