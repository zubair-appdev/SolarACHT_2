import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import QtQuick.Dialogs 1.3


Item {
    id: dataEntryPage
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

            Connections{
                target: keyboard
                function onActiveChanged() {
                    btn_uploadPatch.visible = !keyboard.active
                    btn_uploadPinToPin.visible = !keyboard.active
                    comboFileNames.visible = !keyboard.active
                }
            }

            Text {
                id: titleText
                text: "Data Entry Mode"
                color: "white"
                font.pixelSize: 22
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            TextField {
                id: cableName
                placeholderText: "Enter Cable Name"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferLowercase
            }

            Button {
                id: btn_uploadPatch
                text: "Upload Patch Cable Data"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {

                    if (cableName.text.trim() === "") {
                        msgBox.showYesNo = false
                        msgBox.title = "Error"
                        msgBox.text = "Please enter a cable name."
                        msgBox.open()
                        return
                    }

                    csvDialog.fileType = "patch"   // <---- IMPORTANT
                    csvDialog.open()
                }
            }

            Button {
                id: btn_uploadPinToPin
                text: "Upload Cable Harness Data"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {

                    if (cableName.text.trim() === "") {
                        msgBox.showYesNo = false
                        msgBox.title = "Error"
                        msgBox.text = "Please enter a cable name."
                        msgBox.open()
                        return
                    }

                    csvDialog.fileType = "harness"   // <---- IMPORTANT
                    csvDialog.open()
                }
            }

            ComboBox {
                id: comboFileNames
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                model: loginHandler.getCableNames()

                onActivated: {
                    console.log("Selected:", comboFileNames.currentText)
                }
            }


            Button {
                text: "Get Details"
                width: parent.width * 0.6
                anchors.horizontalCenter: parent.horizontalCenter

                onClicked: {
                    if (comboFileNames.currentIndex === 0) {
                        msgBox.showYesNo = false
                        msgBox.title = "Missing Selection"
                        msgBox.text = "Please select a cable file."
                        msgBox.open()
                        return
                    }

                    var selected = comboFileNames.currentText
                    console.log("Opening details for", selected)
                    stackView.push(Qt.resolvedUrl("DetailsPage.qml"), { cableName: selected, keyboard: keyboard })
                }

            }

            RowLayout {
                width: parent.width * 0.6
                spacing: 20
                anchors.horizontalCenter: parent.horizontalCenter

                Button {
                    text: "Back"
                    Layout.fillWidth: true
                    onClicked: stackView.pop();
                }

                Button {
                    text: "Delete Cable"
                    Layout.fillWidth: true

                    onClicked: {
                        if (comboFileNames.currentIndex === 0) {
                            msgBox.showYesNo = false
                            msgBox.title = "Missing Selection"
                            msgBox.text = "Please select a cable to delete."
                            msgBox.open()
                            return
                        }

                        let selectedCable = comboFileNames.currentText

                        msgBox.showYesNo = true
                        msgBox.title = "Confirm Delete"
                        msgBox.text = "Are you sure you want to delete '" + selectedCable + "'?"

                        msgBox.yesCallback = function() {
                            console.log("YES → deleting:", selectedCable)

                            let ok = loginHandler.deleteCable(selectedCable)

                            // Close the confirm popup first
                            msgBox.close()

                            Qt.callLater(function() {
                                if (ok) {
                                    msgBox.showYesNo = false
                                    msgBox.title = "Deleted"
                                    msgBox.text = "Cable deleted successfully."
                                    msgBox.open()

                                    comboFileNames.model = loginHandler.getCableNames()
                                    comboFileNames.currentIndex = 0
                                } else {
                                    msgBox.showYesNo = false
                                    msgBox.title = "Error"
                                    msgBox.text = "Failed to delete cable."
                                    msgBox.open()
                                }
                            })
                        }


                        msgBox.noCallback = function() { console.log("Delete cancelled") }

                        msgBox.open()
                    }
                }

            }
        }

        FileDialog {
            id: csvDialog
            property string fileType: ""   // patch / harness
            title: "Select CSV File"
            nameFilters: ["CSV Files (*.csv)"]
            selectExisting: true

            onAccepted: {

                let url = fileUrl.toString()

                if (url.length === 0) {
                    msgBox.showYesNo = false
                    msgBox.title = "Error"
                    msgBox.text = "No file selected! Please choose a CSV file."
                    msgBox.open()
                    return
                }

                console.log("Picked file:", url)
                console.log("Type:", fileType)

                let success = loginHandler.processCsvFile(url, cableName.text.trim(), fileType)

                // -------------------------------
                //  ⭐ UPDATE COMBOBOX HERE
                // -------------------------------
                if (success) {
                    comboFileNames.model = loginHandler.getCableNames()
                } else {
                    msgBox.showYesNo = false
                    msgBox.title = "Error"
                    msgBox.text = "Failed to process CSV file."
                    msgBox.open()
                    return
                }

                msgBox.showYesNo = false
                msgBox.title = "Success"
                msgBox.text = "CSV uploaded successfully!"
                msgBox.open()
            }

            onRejected: {
                msgBox.showYesNo = false
                msgBox.title = "Cancelled"
                msgBox.text = "File selection was cancelled."
                msgBox.open()
            }

        }

        MessageBox {
            id: msgBox
        }

    }

}
