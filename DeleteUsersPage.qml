import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Rectangle {
    id: deletePage
    anchors.fill: stackView.view
    color: "#1e1e1e"

    property string currentAdmin: ""

    ListModel {
        id: usersModel
    }

    Component.onCompleted: {
        refreshUsers()
    }

    function refreshUsers() {
        usersModel.clear()
        let users = loginHandler.getAllUsers()

        for (let i = 0; i < users.length; i++) {
            usersModel.append(users[i])
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "Delete Users"
            color: "white"
            font.pixelSize: 26
        }

        ListView {
            id: userList
            width: parent.width * 0.8
            height: 300
            model: usersModel
            spacing: 5

            delegate: Rectangle {
                width: userList.width + 40
                implicitHeight: Math.max(textBlock.implicitHeight + 40, 40)
                color: index % 2 === 0 ? "#2b2b2b" : "#333333"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    // LEFT COLUMN
                    Text {
                        id: textBlock
                        text: username + " (" + role + ")"
                        color: "white"
                        font.pixelSize: 16

                        Layout.fillWidth: true     // 🔥 TAKE ALL FREE SPACE
                        wrapMode: Text.Wrap        // 🔥 IF LONG, WRAP TO NEXT LINE
                    }

                    // RIGHT COLUMN
                    Button {
                        text: "Delete"
                        visible: username !== currentAdmin
                        Layout.preferredWidth: 80  // 🔥 FIXED WIDTH
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        onClicked: confirmDialog.open(username)
                    }
                }
            }
        }

        Button {
            text: "Back"
            width: 200
            onClicked: stackView.pop()
        }
    }

    // Confirmation popup
    Dialog {
        anchors.centerIn: parent
        id: confirmDialog
        modal: true
        property string userToDelete: ""

        function open(user) {
            userToDelete = user
            visible = true
        }

        title: "Confirm Delete"
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: {
            let ok = loginHandler.deleteUser(userToDelete)

            if (ok) {
                refreshUsers()
            }
        }
    }
}
