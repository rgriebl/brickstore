pragma Singleton
import QtQuick
import QtQuick.Window
import BrickStore as BS

QtObject {
    // ~ in mm
    property size physicalScreenSize: Qt.size(Math.min(Screen.width, Screen.height) / Screen.pixelDensity,
                                              Math.max(Screen.width, Screen.height) / Screen.pixelDensity)

    // small is defined as "smaller than 8cm x 12cm"
    property bool smallSize: ((physicalScreenSize.width < 80) || (physicalScreenSize.height < 120)
                              || (uiSize === BS.Config.UISize.Small))
                             && !(uiSize === BS.Config.UISize.Large)

    property int uiSize: BS.Config.mobileUISize

    property color accentColor: Qt.color("blue") //TODO: from Material or Basic or iOS

    function dimColor(baseColor) {
    }
}
