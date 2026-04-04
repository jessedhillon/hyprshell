pragma ComponentBehavior: Bound

import QtQuick
import qs.components
import qs.services
import qs.config

StyledRect {
    id: root

    readonly property color colour: Colours.palette.m3tertiary
    readonly property int padding: Appearance.padding.small

    implicitWidth: Config.bar.sizes.innerWidth
    implicitHeight: layout.implicitHeight + root.padding * 2

    color: "transparent"
    radius: Appearance.rounding.full

    Column {
        id: layout

        anchors.centerIn: parent
        spacing: Appearance.spacing.small

        MaterialIcon {
            anchors.horizontalCenter: parent.horizontalCenter

            text: "calendar_month"
            color: root.colour
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter

            horizontalAlignment: StyledText.AlignHCenter
            text: Time.format("MM")
            font.pointSize: Appearance.font.size.smaller
            font.family: Appearance.font.family.mono
            font.weight: 700
            color: root.colour
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter

            horizontalAlignment: StyledText.AlignHCenter
            text: Time.format("dd")
            font.pointSize: Appearance.font.size.smaller
            font.family: Appearance.font.family.mono
            font.weight: 300
            color: root.colour
        }
    }
}
