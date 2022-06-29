import Mobile
import BrickStore as BS


Control {
    id: root

    property bool darkTheme: Material.theme === Material.Dark
    property color accentColor: Material.accentColor
    property color accentTextColor: Material.primaryHighlightedTextColor
    property color backgroundColor: Material.backgroundColor
    property color textColor: Material.foreground
    property color primaryColor: Material.primaryColor
    property color primaryTextColor: Material.primaryTextColor
    property color primaryHighlightedTextColor: Material.primaryHighlightedTextColor
    property color hintTextColor: Material.hintTextColor

    visible: false

    function updateTheme() {
        let t = Material.System

        switch (BS.Config.uiTheme) {
        case BS.Config.UITheme.Light: t = Material.Light; break
        case BS.Config.UITheme.Dark: t = Material.Dark; break
        }
        Window.window.Material.theme = t

        BS.BrickStore.updateIconTheme(Material.theme === Material.Dark)
    }

    Connections {
        target: BS.Config
        function onUiThemeChanged() { root.updateTheme() }
    }

    Component.onCompleted: { updateTheme() }
}
