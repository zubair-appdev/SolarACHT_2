import QtQuick 2.14
import QtQuick.Controls 2.14

Popup {
    id: popup
    modal: true
    focus: true
    dim: true

    width: parent ? parent.width * 0.6 : 300
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    background: Rectangle {
        color: "#2a2a2a"
        radius: 10
    }

    //----------------------------------------------------------------------
    // Public properties
    //----------------------------------------------------------------------
    property alias text: message.text
    property alias title: titleText.text

    property bool showYesNo: false    // false → OK mode, true → Yes/No mode

    // Callback functions (set by caller)
    property var okCallback: null
    property var yesCallback: null
    property var noCallback: null

    //----------------------------------------------------------------------
    // INTERNAL UI
    //----------------------------------------------------------------------
    Column {
        spacing: 15
        padding: 20
        width: popup.width

        Text {
            id: titleText
            color: "white"
            font.pixelSize: 22
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            id: message
            color: "lightgray"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        //------------------------------------------------------------------
        // OK BUTTON (used when showYesNo == false)
        //------------------------------------------------------------------
        Button {
            visible: !popup.showYesNo
            text: "OK"
            anchors.horizontalCenter: parent.horizontalCenter
            width: popup.width * 0.4

            onClicked: {
                if (popup.okCallback)
                    popup.okCallback()

                // Reset callbacks
                popup.okCallback = null
                popup.yesCallback = null
                popup.noCallback = null

                popup.close()
            }
        }

        //------------------------------------------------------------------
        // YES / NO BUTTONS (used when showYesNo == true)
        //------------------------------------------------------------------
        Row {
            visible: popup.showYesNo
            spacing: 20
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: "Yes"
                width: popup.width * 0.3

                onClicked: {
                    if (popup.yesCallback)
                        popup.yesCallback()

                    popup.okCallback = null
                    popup.yesCallback = null
                    popup.noCallback = null
                    popup.close()
                }
            }

            Button {
                text: "No"
                width: popup.width * 0.3

                onClicked: {
                    if (popup.noCallback)
                        popup.noCallback()

                    popup.okCallback = null
                    popup.yesCallback = null
                    popup.noCallback = null
                    popup.close()
                }
            }
        }
    }
}
