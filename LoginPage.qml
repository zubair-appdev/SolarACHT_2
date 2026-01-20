import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Item {
    id: loginPage
    anchors.fill: stackView.view

    // whenever this page is shown, clear fields
      onVisibleChanged: {
          if (visible) {
              usernameField.text = ""
              passwordField.text = ""
              errorText.visible = false
          }
      }

    // property to receive keyboard reference
    property Item keyboard

    Rectangle {
        anchors.fill: parent
        color: "#2d2d30"

        Column {
            id: loginColumn
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 15
            //  move up when keyboard appears
            anchors.verticalCenterOffset: keyboard && keyboard.active ? -120 : 0

            Behavior on anchors.verticalCenterOffset {
                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }

            Text {
                text: "Login"
                color: "white"
                font.pixelSize: 28
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Row {
                spacing: 10
                anchors.horizontalCenter: parent.horizontalCenter

                TextField {
                    id: usernameField
                    width: 150
                    placeholderText: "Username"
                    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase
                }

                TextField {
                    id: passwordField
                    width: 150
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                }
            }

            Button {
                text: "Login"
                width: 240
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {
                    let u = usernameField.text.trim()
                    let p = passwordField.text.trim()

                    if (u === "" || p === "") {
                        errorText.text = "⚠️ Please fill all fields"
                        errorText.visible = true
                        return
                    }

                    // ONE backend call
                    let role = loginHandler.loginUser(u, p)

                    if (role !== "") {
                        errorText.visible = false

                        stackView.push(Qt.resolvedUrl("HomePage.qml"), {
                            keyboard:keyboard,
                            username: u,
                            role: role
                        })
                    } else {
                        errorText.text = "❌ Invalid username or password"
                        errorText.visible = true
                    }
                }
            }


            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                Button {
                    id: newUserButton
                    text: "New User"
                    width: 150
                    onClicked: {
                        stackView.push(Qt.resolvedUrl("SelectRolePage.qml"), { keyboard: keyboard, page: "selectRole"})
                    }
                }


                Button {
                    id: forgotPasswordButton
                    text: "Forgot Password"
                    width: 150
                    onClicked: {
                        stackView.push(Qt.resolvedUrl("SelectRolePage.qml"), { keyboard: keyboard, page: "forgotPassword" })
                    }
                }
            }

            Text {
                id: errorText
                text: "Invalid Username or Password"
                color: "red"
                font.pixelSize: 16
                visible: false
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
