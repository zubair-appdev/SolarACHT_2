// TestPage.qml
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14

Item {
    id: testPage
    anchors.fill: stackView.view

    property Item keyboard

    property var portModel: testController.availablePorts()

    property bool portConnected: false

    property string sendCheck: simulationCheck.checked ? "Go" : "No"

    Rectangle{
        anchors.fill: parent
        color: "#1e1e1e"

        ColumnLayout{
            width: parent.width
            height: parent.height
            spacing: 20

            RowLayout{
                width: parent.width
                Layout.topMargin: 10

                Item{ Layout.fillWidth: true }

                Text {
                    id: titleText
                    text: "Test Mode"
                    color: "white"
                    font.pixelSize: 22
                }

                Item{ Layout.fillWidth: true }
            }

            RowLayout{
                width: parent.width

                Item{ Layout.minimumWidth:  20 }

                ComboBox{
                    id: selectFile
                    model: loginHandler.getCableNames()
                }

                TextField{
                    id: setNo
                    placeholderText: "Enter Set No"
                    inputMethodHints: Qt.ImhNoPredictiveText
                }

                TextField{
                    id: performedBy
                    placeholderText: "Performed By"
                    inputMethodHints: Qt.ImhNoPredictiveText
                }

                TextField{
                    id: inspectedBy
                    placeholderText: "Inspected By"
                    inputMethodHints: Qt.ImhNoPredictiveText
                }

                Item{ Layout.minimumWidth:  20 }
            }

            RowLayout{
                width: parent.width

                Item{ Layout.minimumWidth:  20 }

                TextField{
                    id: projectName
                    placeholderText: "Project Name"
                    inputMethodHints: Qt.ImhNoPredictiveText
                    Layout.fillWidth: true
                }

                TextField{
                    id: notes
                    placeholderText: "Notes"
                    inputMethodHints: Qt.ImhNoPredictiveText
                    Layout.fillWidth: true
                }

                Item{ Layout.minimumWidth:  20 }
            }

            RowLayout{

                Item{ Layout.fillWidth :  true }

                ComboBox{
                    Layout.preferredWidth: 300
                    id: testToRun
                    model: ["Test","Two Wire Continuity","Isolation","Insulation"]
                }

                ComboBox{
                    id: portSelect
                    model: portModel
                }

                Button{
                    id: connectBtn
                    text: portConnected ? "Disconnect" : "Connect"

                    onClicked: {
                        if (!portConnected) {

                            if (portSelect.currentIndex === 0) {
                                msgBox.showYesNo = false
                                msgBox.title = "Port Error"
                                msgBox.text = "Please select a valid port"
                                msgBox.open()
                                return
                            }

                            if (testController.connectPort(portSelect.currentText)) {
                                portConnected = true
                            } else {
                                msgBox.showYesNo = false
                                msgBox.title = "Connection Failed"
                                msgBox.text = "Unable to open selected port"
                                msgBox.open()
                            }

                        } else {
                            testController.disconnectPort()
                            portConnected = false
                        }
                    }
                }

                Button{
                    text: "Refresh"
                    onClicked: {
                        testController.disconnectPort()
                        portConnected = false

                        portModel = testController.availablePorts()
                        portSelect.currentIndex = 0
                    }
                }

                CheckBox{
                    id: simulationCheck
                    text: "<font color='white' size = '5'>Simulate</font>"

                    background: Rectangle {
                           border.color: "white"
                           border.width: 2
                           radius: 4
                           color: "transparent"
                       }
                }


                Item{ Layout.fillWidth :  true }
            }

            Item{ Layout.fillHeight :  true }

            RowLayout{
                width: parent.width
                Layout.bottomMargin: 20

                Item{ Layout.minimumWidth:  20 }

                Button{
                    text: "Back"
                    Layout.fillWidth :  true

                    onClicked: {
                        if (portConnected) {
                                testController.disconnectPort()
                                portConnected = false
                            }
                        stackView.pop()
                    }
                }

                Button{
                    text: "Run"
                    Layout.fillWidth :  true

                    onClicked: {
                        if (!testController.isConnected()) {
                            msgBox.showYesNo = false
                            msgBox.title = "Port Not Connected"
                            msgBox.text = "Please connect to hardware before running test"
                            msgBox.open()
                            return
                        }


                        if(selectFile.currentIndex === 0 || setNo.text === "" ||
                                performedBy.text === "" || inspectedBy.text === "" ||
                                projectName.text === "" || notes.text === "" ||
                                testToRun.currentIndex === 0)
                        {
                            msgBox.showYesNo = false
                            msgBox.title = "Missing Field"
                            msgBox.text = "Please fill all details,select file and choose test"
                            msgBox.open()
                            return
                        }                       
                        else {
                            // 1. Read selected cable name
                            const cableName = selectFile.currentText;

                            // 2. Fetch Patch + Harness Data from backend
                            let patch = loginHandler.getPatchData(cableName);
                            let harness = loginHandler.getHarnessData(cableName);

                            if (patch.length === 0 || harness.length === 0) {
                                msgBox.showYesNo = false
                                msgBox.title = "Data Missing"
                                msgBox.text = "No Patch/Harness data found for this cable!"
                                msgBox.open()
                                return
                            }

                            // 3. Save metadata + raw data (logging purpose)
                            loginHandler.prepareTestData(
                                        cableName,
                                        setNo.text,
                                        performedBy.text,
                                        inspectedBy.text,
                                        projectName.text,
                                        notes.text,
                                        testToRun.currentText,
                                        patch,
                                        harness
                                        );

                            // 4. Mapping (common for all tests)
                            if (!testController.mapLogicalToHardware(patch, harness)) {
                                msgBox.title = "Mapping Failed"
                                msgBox.text = "Patch ↔ Harness mapping failed."
                                msgBox.showYesNo = false
                                msgBox.open()
                                return
                            }

                            // Simulation purpose
                            let simulate = "No"
                            if(sendCheck === "Go")
                            {
                                simulate = "Go"
                            }

                            console.log(simulate+" :simulate QML")

                            // 5. Branch based on selected test
                            if (testToRun.currentText === "Two Wire Continuity") {
                                // 👉 ONLY here Two Wire packet is generated & sent
                                if (!testController.runTwoWireContinuity(simulate)) {
                                    msgBox.title = "Two Wire Failed"
                                    msgBox.text = "Failed to send Two Wire Continuity packet."
                                    msgBox.open()
                                    return
                                }

                                msgBox.title = "Two Wire Started"
                                msgBox.text  = "Two Wire Continuity test started successfully."
                                msgBox.open()
                            }
                            else if (testToRun.currentText === "Isolation")
                            {
                                if (!testController.runIsolation(simulate)) {
                                    msgBox.title = "Isolation Failed"
                                    msgBox.text  = "Failed to start Isolation Test"
                                    msgBox.open()
                                    return
                                }

                                msgBox.title = "Isolation Started"
                                msgBox.text  = "Isolation test started successfully"
                                msgBox.open()
                            }
                            else if (testToRun.currentText === "Insulation")
                            {
                                if (!testController.runInsulation(simulate)) {
                                    msgBox.title = "Insulation Failed"
                                    msgBox.text  = "Failed to start Insulation Test"
                                    msgBox.open()
                                    return
                                }

                                msgBox.title = "Insulation Started"
                                msgBox.text  = "Insulation test started successfully"
                                msgBox.open()
                            }
                            else
                            {
                                console.log("Invalid Test Pressed !!!");
                            }

                        }

                    }
                }

                Item{ Layout.minimumWidth:  20 }
            }
        }

        Connections {
            target: testController

            function onPortStatusChanged(connected, msg) {
                msgBox.showYesNo = false
                msgBox.title = connected ? "Connected" : "Disconnected"
                msgBox.text = msg
                msgBox.open()
            }

            function onTwoWireFinished(success, message) {
                    if (success) {
                        msgBox.title = "Success"
                        msgBox.text  = message
                    } else {
                        msgBox.title = "Failure"
                        msgBox.text  = message
                    }
                    msgBox.showYesNo = false
                    msgBox.open()
                }
        }

        MessageBox {
            id: msgBox
        }
    }
}
