import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Rectangle {
    id: homePage
    anchors.fill: stackView.view
    color: "#1e1e1e"

    property Item keyboard

    property string username: ""
    property string role: ""

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "Welcome, " + username + "!"
            color: "white"
            font.pixelSize: 26
        }

        Text {
            text: "Role: " + role
            color: "lightgray"
            font.pixelSize: 20
        }

        // ADMIN-ONLY SECTION STARTS
        Button {
            text: "Delete Users"
            width: 200
            visible: role === "Admin"
            onClicked: {
                stackView.push(Qt.resolvedUrl("DeleteUsersPage.qml"), {
                    currentAdmin: username
                })
            }
        }

        Button{
            text: "Data Entry"
            width: 200
            visible: role === "Admin"

            onClicked: {
                stackView.push(Qt.resolvedUrl("DataEntryPage.qml"),{keyboard:keyboard})
            }
        }
        // ADMIN-ONLY SECTION ENDS

        Button{
            text: "Test"
            width: 200

            onClicked:
            {
                stackView.push(Qt.resolvedUrl("TestPage.qml"), {keyboard:keyboard})
            }

        }


        Button {
            text: "Logout"
            width: 200
            onClicked: stackView.pop(stackView.initialItem)
        }
    }
}
