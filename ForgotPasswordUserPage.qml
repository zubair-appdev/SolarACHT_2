import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Item {
    id: forgotPage
    anchors.fill: stackView.view

    property Item keyboard

    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"

        Column{
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            spacing: 20

            // move up when keyboard appears
            anchors.verticalCenterOffset: keyboard && keyboard.active ? -parent.height * 0.3 : 0

            Behavior on anchors.verticalCenterOffset {
                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }

            Connections
            {
                target: keyboard
                function onActiveChanged() {
                    statusText.visible = !keyboard.active
                }
            }

            Text {
                id: titleText
                text: "Admin login required for recovering user password"
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

            TextField {
                id: password
                placeholderText: "Password"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase
                echoMode: TextInput.Password
            }

            Button {
                text: "Validate"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
                    if(username.text.trim() === "" || password.text.trim() === "")
                    {
                        statusText.text = "⚠️ Please fill all fields."
                        statusText.color = "orange"
                        statusText.visible = true
                        return
                    }


                    if (loginHandler.validateAdmin(username.text.trim(),
                                                   password.text.trim()))
                    {
                        console.log("Correct Admin!")
                        statusText.visible = false

                        stackView.push(Qt.resolvedUrl("UpdateAdminUserPage.qml"),
                                       { role: "User", keyboard: keyboard })
                    }
                    else
                    {
                        statusText.text = "⚠️ Wrong credentials entered !"
                        statusText.color = "red"
                        statusText.visible = true
                    }
                }
            }

            Button {
                text: "Back"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: stackView.pop()
            }

            Text {
                id: statusText
                text: ""
                color: "white"
                font.pixelSize: 18
                visible: true
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

    }

}
