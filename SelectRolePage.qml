// SelectRolePage.qml
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Item {
    id: selectRolePage
    anchors.fill: stackView.view

    property Item keyboard

    property string page: ""

    Rectangle {
        anchors.fill: parent
        color: "#2b2b2b"

        Column {
            anchors.centerIn: parent
            spacing: 25

            Text {
                text: "Select Role"
                color: "white"
                font.pixelSize: 26
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Row {
                spacing: 20
                anchors.horizontalCenter: parent.horizontalCenter

                Button {
                    text: "Admin"
                    width: 120
                    onClicked: {   
                        if(page === "selectRole")
                        {
                            stackView.push(Qt.resolvedUrl("CreateUserPage.qml"),
                                           { role: "Admin", type: "newMaster" , keyboard: keyboard })
                        }
                        else if(page === "forgotPassword")
                        {
                            stackView.push(Qt.resolvedUrl("ForgotPasswordAdminPage.qml"),
                                           { keyboard: keyboard })
                        }
                        else
                        {
                            console.log("Unknown Page");
                        }
                    }
                }

                Button {
                    text: "User"
                    width: 120
                    onClicked: {
                        if(page === "selectRole")
                        {
                            stackView.push(Qt.resolvedUrl("CreateUserPage.qml"),
                                           { role: "User", keyboard: keyboard })
                        }
                        else if(page === "forgotPassword")
                        {
                            stackView.push(Qt.resolvedUrl("ForgotPasswordUserPage.qml"),
                                           { keyboard: keyboard })
                        }
                        else
                        {
                            console.log("Unknown Page");
                        }
                    }
                }

            }

            Button {
                text: "Back"
                width: 200
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: stackView.pop()
            }
        }
    }
}
