pragma ComponentBehavior: Bound

import QtQuick
import Caelestia.Terminal
import qs.components
import qs.config

Item {
    id: root

    implicitWidth: Config.bar.sizes.innerWidth

    TerminalView {
        anchors.fill: parent
        fontFamily: Appearance.font.family.mono
        fontSize: Appearance.font.size.smaller
        focus: true
    }
}
