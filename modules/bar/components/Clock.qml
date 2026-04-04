pragma ComponentBehavior: Bound

import QtQuick
import qs.components
import qs.services
import qs.config

StyledRect {
    id: root

    readonly property color colour: Colours.palette.m3tertiary
    readonly property int padding: Config.bar.clock.background ? Appearance.padding.normal : Appearance.padding.small

    implicitWidth: Config.bar.sizes.innerWidth
    implicitHeight: layout.implicitHeight + root.padding * 2

    color: Qt.alpha(Colours.tPalette.m3surfaceContainer, Config.bar.clock.background ? Colours.tPalette.m3surfaceContainer.a : 0)
    radius: Appearance.rounding.full

    Column {
        id: layout

        anchors.centerIn: parent
        spacing: Appearance.spacing.small

        Loader {
            asynchronous: true
            anchors.horizontalCenter: parent.horizontalCenter

            active: Config.bar.clock.showIcon
            visible: active

            sourceComponent: MaterialIcon {
                text: "schedule"
                color: root.colour
            }
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter

            visible: Config.bar.clock.showDate

            horizontalAlignment: StyledText.AlignHCenter
            text: Time.format("ddd\nd")
            font.pointSize: Appearance.font.size.smaller
            font.family: Appearance.font.family.sans
            color: root.colour
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: Config.bar.clock.showDate
            height: visible ? 1 : 0

            width: parent.width * 0.8
            color: root.colour
            opacity: 0.2
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter

            horizontalAlignment: StyledText.AlignHCenter
            text: Time.format("HH")
            font.pointSize: Appearance.font.size.smaller
            font.family: Appearance.font.family.mono
            font.weight: 700
            color: root.colour
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter

            horizontalAlignment: StyledText.AlignHCenter
            text: Time.format("mm")
            font.pointSize: Appearance.font.size.smaller
            font.family: Appearance.font.family.mono
            font.weight: 300
            color: root.colour
        }
    }
}
