import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Item {
    id: detailsPage
    anchors.fill: stackView.view

    property string cableName: ""

    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"

        Column {
            anchors.fill: parent
            spacing: 6
            padding: 6

            //---------------------------------------------------------
            // HEADER ROW
            //---------------------------------------------------------
            RowLayout {
                width: parent.width
                spacing: 10

                Label { text: "Details for:"; color: "white"; font.pixelSize: 20 }
                Text { text: cableName; color: "white"; font.pixelSize: 20; font.bold: true }
                Item { Layout.fillWidth: true }
                Button { text: "Back"; onClicked: stackView.pop() }
            }

            //---------------------------------------------------------
            // TITLE ABOVE PATCH TABLE
            //---------------------------------------------------------
            Text {
                text: "Patch Data"
                color: "white"
                font.pixelSize: 18
                font.bold: true
                anchors.left: parent.left
            }

            //---------------------------------------------------------
            // PATCH SECTION
            //---------------------------------------------------------
            Rectangle {
                width: parent.width
                height: parent.height * 0.32
                radius: 6
                color: "#2a2a2a"
                border.color: "#555"

                Flickable {
                    anchors.fill: parent
                    clip: true
                    contentWidth: parent.width
                    contentHeight: patchColumn.height

                    Column {
                        id: patchColumn
                        width: parent.width
                        spacing: 4

                        // Header Row
                        Rectangle {
                            width: parent.width
                            height: 28
                            color: "#333"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6

                                Text { text: "Cable"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.20 }
                                Text { text: "User Con"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.25 }
                                Text { text: "User Pin"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.10 }
                                Text { text: "Patch Con"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.25 }
                                Text { text: "Patch Pin"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.10 }
                            }
                        }

                        // Data Rows
                        Repeater {
                            model: patchModel
                            delegate: Rectangle {
                                width: parent.width
                                color: index % 2 === 0 ? "#2d2d2d" : "#262626"

                                implicitHeight: rowLayout.implicitHeight + 8

                                RowLayout {
                                    id: rowLayout
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6

                                    Text {
                                        text: model.cable
                                        color: "white"
                                        wrapMode: Text.WordWrap
                                        Layout.preferredWidth: parent.width * 0.20
                                    }

                                    Text {
                                        text: model.userCon
                                        color: "white"
                                        wrapMode: Text.WordWrap
                                        Layout.preferredWidth: parent.width * 0.25
                                    }

                                    Text { text: model.userPin; color: "white"; wrapMode: Text.WordWrap; Layout.preferredWidth: parent.width * 0.10 }

                                    Text {
                                        text: model.patchCon
                                        color: "white"
                                        wrapMode: Text.WordWrap
                                        Layout.preferredWidth: parent.width * 0.25
                                    }

                                    Text { text: model.patchPin; color: "white"; wrapMode: Text.WordWrap; Layout.preferredWidth: parent.width * 0.10 }
                                }
                            }
                        }
                    }
                }
            }

            //---------------------------------------------------------
            // TITLE ABOVE HARNESS TABLE
            //---------------------------------------------------------
            Text {
                text: "Harness Data"
                color: "white"
                font.pixelSize: 18
                font.bold: true
                anchors.left: parent.left
            }

            //---------------------------------------------------------
            // HARNESS SECTION
            //---------------------------------------------------------
            Rectangle {
                width: parent.width
                height: parent.height * 0.32
                radius: 6
                color: "#2a2a2a"
                border.color: "#555"

                Flickable {
                    anchors.fill: parent
                    clip: true
                    contentWidth: parent.width
                    contentHeight: harnessColumn.height

                    Column {
                        id: harnessColumn
                        width: parent.width
                        spacing: 4

                        // Header Row
                        Rectangle {
                            width: parent.width
                            height: 28
                            color: "#333"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6

                                Text { text: "Cable"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.12 }
                                Text { text: "Source Con"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.26 }
                                Text { text: "Src Pin"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.08 }
                                Text { text: "Dest Con"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.26 }
                                Text { text: "Dst Pin"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.08 }
                                Text { text: "Exp"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.08 }
                                Text { text: "Volt"; color: "white"; font.bold: true; Layout.preferredWidth: parent.width * 0.08 }
                            }
                        }

                        // Data Rows
                        Repeater {
                            model: harnessModel
                            delegate: Rectangle {
                                width: parent.width
                                color: index % 2 === 0 ? "#2d2d2d" : "#262626"

                                implicitHeight: rowLayout2.implicitHeight + 8

                                RowLayout {
                                    id: rowLayout2
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6

                                    Text {
                                        text: model.cable
                                        color: "white"
                                        wrapMode: Text.WordWrap
                                        Layout.preferredWidth: parent.width * 0.12
                                    }

                                    Text {
                                        text: model.sourceCon
                                        color: "white"
                                        wrapMode: Text.WordWrap
                                        Layout.preferredWidth: parent.width * 0.26
                                    }

                                    Text { text: model.sourcePin; color: "white";wrapMode: Text.WordWrap; Layout.preferredWidth: parent.width * 0.08 }

                                    Text {
                                        text: model.destCon
                                        color: "white"
                                        wrapMode: Text.WordWrap
                                        Layout.preferredWidth: parent.width * 0.26
                                    }

                                    Text { text: model.destPin; color: "white"; wrapMode: Text.WordWrap; Layout.preferredWidth: parent.width * 0.08 }
                                    Text { text: model.exp; color: "white"; wrapMode: Text.WordWrap; Layout.preferredWidth: parent.width * 0.08 }
                                    Text { text: model.volt; color: "white"; wrapMode: Text.WordWrap; Layout.preferredWidth: parent.width * 0.08 }
                                }
                            }
                        }
                    }
                }
            }
        }

        //---------------------------------------------------------
        // MODELS
        //---------------------------------------------------------
        ListModel { id: patchModel }
        ListModel { id: harnessModel }

        Component.onCompleted: {
            if (cableName === "") return

            patchModel.clear()
            for (var p of loginHandler.getPatchData(cableName))
                patchModel.append(p)

            harnessModel.clear()
            for (var h of loginHandler.getHarnessData(cableName))
                harnessModel.append(h)
        }
    }
}
