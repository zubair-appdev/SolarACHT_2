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

            Text {
                id: titleText
                text: "Master Key"
                color: "white"
                font.pixelSize: 26
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            TextField {
                id: masterKey
                placeholderText: "Enter Master Key"
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
                    if(masterKey.text.trim() === "")
                    {
                        statusText.text = "⚠️ Please fill Master Key."
                        statusText.color = "orange"
                        statusText.visible = true
                        return
                    }
                    else if(masterKey.text.trim() === "maytech1234")
                    {
                        console.log("Correct Master Key!")
                        statusText.visible = false

                        stackView.push(Qt.resolvedUrl("UpdateAdminUserPage.qml"),
                                       { role: "Admin", keyboard: keyboard })

                    }
                    else
                    {
                        statusText.text = "⚠️ Wrong master key entered !"
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
