import QtQuick
import ".."

// Small uppercase label. Letter-spacing is what makes it read as deliberate.
Text {
    id: root
    required property var appWindow
    textFormat: Text.PlainText
    color: Theme.txtDim
    font.pixelSize: 10
    font.weight: Font.DemiBold
    font.capitalization: Font.AllUppercase
}
