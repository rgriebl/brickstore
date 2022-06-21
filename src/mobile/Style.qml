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

    property bool darkTheme: styleHelper.darkTheme
    property color accentColor: styleHelper.accentColor
    property color accentTextColor: styleHelper.accentTextColor
    property color backgroundColor: styleHelper.backgroundColor
    property color textColor: styleHelper.textColor
    property color primaryColor: styleHelper.primaryColor
    property color primaryTextColor: styleHelper.primaryTextColor
    property color primaryHighlightedTextColor: styleHelper.primaryHighlightedTextColor
    property color hintTextColor: styleHelper.hintTextColor

    property var styleHelper

}
