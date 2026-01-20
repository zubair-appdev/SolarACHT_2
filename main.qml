import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14
import QtQuick.VirtualKeyboard 2.14
import QtQuick.Layouts 1.14

Window {
    id: window
    visible: true
    title: qsTr("Automatic Cable Harness Tester")
    color: "#101010" // optional dark background for touch UI

    //  Auto-fit to actual screen size
    width: 800
    height: 480

    //  Virtual Keyboard
    InputPanel {
        id: inputPanel
        z: 99
        x: 0
        y: window.height
        width: window.width

        states: State {
            name: "visible"
            when: inputPanel.active
            PropertyChanges {
                target: inputPanel
                y: window.height - inputPanel.height
            }
        }

        transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            ParallelAnimation {
                NumberAnimation {
                    properties: "y"
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    //  StackView for navigation (adaptive)
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: LoginPage {
            keyboard: inputPanel   // pass the virtual keyboard reference
        }
    }
}
