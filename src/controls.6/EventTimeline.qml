import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui
import org.mauikit.calendar 1.0 as MauiCalendar

Item
{
    id: control

    property date startDate: new Date()
    property int dayCount: 1
    property var filter: MauiCalendar.Filter
    property date currentTime: new Date()

    Timer
    {
        interval: 1000
        repeat: true
        running: true
        onTriggered: control.currentTime = new Date()
    }

    signal incidenceActivated(var incidence, var occurrence)
    signal timeSlotActivated(var date)
    signal dateActivated(var date)

    readonly property real hourHeight: Maui.Style.units.gridUnit * 3
    readonly property real gutterWidth: Maui.Style.units.gridUnit * 4
    readonly property real minimumDayWidth: Maui.Style.units.gridUnit * 8

    Flickable
    {
        anchors.fill: parent
        contentWidth: Math.max(width, control.gutterWidth + control.dayCount * control.minimumDayWidth)
        contentHeight: timeline.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout
        {
            id: timeline
            width: Math.max(parent.width, control.gutterWidth + control.dayCount * control.minimumDayWidth)
            spacing: Maui.Style.space.tiny - Maui.Style.space.tiny

            RowLayout
            {
                visible: control.dayCount > 1
                Layout.fillWidth: true
                spacing: Maui.Style.space.tiny - Maui.Style.space.tiny

                Item { Layout.preferredWidth: control.gutterWidth }

                Repeater
                {
                    model: control.dayCount

                    delegate: Item
                    {
                        required property int index
                        Layout.fillWidth: true
                        Layout.minimumWidth: control.minimumDayWidth
                        Layout.preferredHeight: dayHeaderContent.implicitHeight + Maui.Style.space.tiny * 2

                        ColumnLayout
                        {
                            id: dayHeaderContent
                            anchors.centerIn: parent
                            spacing: Maui.Style.space.tiny
                            readonly property date headerDate: new Date(control.startDate.getFullYear(), control.startDate.getMonth(), control.startDate.getDate() + index)

                            Label
                            {
                                Layout.alignment: Qt.AlignHCenter
                                text: Qt.formatDate(dayHeaderContent.headerDate, "ddd")
                                font.weight: Font.Medium
                                color: Maui.Theme.textColor
                            }

                            Label
                            {
                                Layout.alignment: Qt.AlignHCenter
                                text: Qt.formatDate(dayHeaderContent.headerDate, "d")
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: Maui.Style.iconSize
                                Layout.preferredHeight: Maui.Style.iconSize
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                background: Rectangle
                                {
                                    anchors.fill: parent
                                    visible: Qt.formatDate(dayHeaderContent.headerDate, "yyyyMMdd") === Qt.formatDate(control.currentTime, "yyyyMMdd")
                                    radius: width / 2
                                    color: Maui.Theme.highlightColor
                                }
                                color: Qt.formatDate(dayHeaderContent.headerDate, "yyyyMMdd") === Qt.formatDate(control.currentTime, "yyyyMMdd") ? Maui.Theme.highlightedTextColor : Maui.Theme.textColor
                            }
                        }
                    }
                }
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: Maui.Style.space.tiny - Maui.Style.space.tiny

                Item
                {
                    Layout.preferredWidth: control.gutterWidth
                    Layout.preferredHeight: 24 * control.hourHeight + Maui.Style.rowHeight / 2

                    Repeater
                    {
                        model: 25

                        delegate: Label
                        {
                            required property int index
                            y: index * control.hourHeight - implicitHeight / 2
                            width: parent.width
                            text: index === 24 ? "24:00" : Qt.formatTime(new Date(2000, 0, 1, index), "HH:mm")
                            horizontalAlignment: Text.AlignRight
                            color: Maui.Theme.textColor
                        }
                    }
                }

                Repeater
                {
                    model: control.dayCount

                    delegate: Item
                    {
                        id: dayColumn
                        required property int index
                        Layout.fillWidth: true
                        Layout.minimumWidth: control.minimumDayWidth
                        Layout.preferredHeight: 24 * control.hourHeight
                        clip: true

                        Rectangle
                        {
                            anchors.left: parent.left
                            height: parent.height
                            width: Maui.Style.units.smallSpacing / 2
                            color: Qt.alpha(Maui.Theme.textColor, 0.28)
                            z: 1
                        }

                        property date dayDate: new Date(control.startDate.getFullYear(), control.startDate.getMonth(), control.startDate.getDate() + index)
                        readonly property bool isToday: Qt.formatDate(dayDate, "yyyyMMdd") === Qt.formatDate(control.currentTime, "yyyyMMdd")

                        Rectangle
                        {
                            id: currentTimeLine
                            visible: dayColumn.isToday
                            x: currentTimeDot.width / 2
                            y: (control.currentTime.getHours() + control.currentTime.getMinutes() / 60 + control.currentTime.getSeconds() / 3600) * control.hourHeight
                            width: parent.width - currentTimeDot.width / 2
                            height: Maui.Style.units.smallSpacing / 2
                            color: "#f2b8b8"
                            z: 3

                            Rectangle
                            {
                                id: currentTimeDot
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                x: -width / 2
                                width: Maui.Style.iconSize / 3
                                height: width
                                radius: width / 2
                                color: "#f2b8b8"
                            }
                        }

                        MauiCalendar.IncidenceOccurrenceModel
                        {
                            id: dayEvents
                            start: dayColumn.dayDate
                            length: 1
                            filter: control.filter
                        }

                        MauiCalendar.HourlyIncidenceModel
                        {
                            id: hourlyEvents
                            model: dayEvents
                            filters: MauiCalendar.HourlyIncidenceModel.NoAllDay
                        }

                        Repeater
                        {
                            z: -1
                            model: dayEvents

                            delegate: Item
                            {
                                id: allDayBackground
                                required property bool allDay
                                required property var color
                                visible: allDay
                                anchors.fill: parent

                                Rectangle
                                {
                                    anchors.fill: parent
                                    color: Qt.alpha(allDayBackground.color, 0.10)
                                    border.color: Qt.alpha(allDayBackground.color, 0.48)
                                    border.width: Maui.Style.units.smallSpacing / 4
                                }
                            }
                        }

                        Column
                        {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            z: 2
                            spacing: Maui.Style.space.tiny
                            clip: true

                            Repeater
                            {
                                model: dayEvents

                                delegate: Maui.ListDelegate
                                {
                                    id: allDayLabel
                                    required property bool allDay
                                    required property string summary
                                    required property var color
                                    required property var incidencePtr
                                    required property var startTime
                                    visible: allDay
                                    width: parent.width
                                    height: allDay ? Maui.Style.rowHeight : 0
                                    label: summary
                                    background: Rectangle
                                    {
                                        color: Qt.alpha(allDayLabel.color, 0.24)
                                        border.color: allDayLabel.color
                                        border.width: Maui.Style.units.smallSpacing / 4
                                    }
                                    onClicked: control.incidenceActivated(incidencePtr, startTime)
                                }
                            }
                        }

                        Repeater
                        {
                            model: 24

                            delegate: Rectangle
                            {
                                required property int index
                                y: index * control.hourHeight
                                width: parent.width
                                height: Maui.Style.units.smallSpacing / 2
                                color: Qt.alpha(Maui.Theme.textColor, 0.28)
                            }
                        }

                        Repeater
                        {
                            z: 1
                            model: hourlyEvents

                            delegate: Item
                            {
                                required property var incidences
                                anchors.fill: parent

                                Repeater
                                {
                                    model: parent.incidences

                                    delegate: Rectangle
                                    {
                                        required property var modelData
                                        x: parent.width * Number(modelData.priorTakenWidthShare || 0)
                                        y: Number(modelData.starts) * control.hourHeight / 4
                                        width: parent.width * Number(modelData.widthShare || 1)
                                        height: Math.max(Maui.Style.units.gridUnit * 1.5, Number(modelData.duration) * control.hourHeight / 4)
                                        visible: !modelData.allDay
                                        clip: true
                                        radius: Maui.Style.radiusV
                                        color: Qt.alpha(modelData.color, 0.28)
                                        border.color: modelData.color
                                        border.width: Maui.Style.units.smallSpacing / 4

                                        Column
                                        {
                                            anchors.fill: parent
                                            anchors.margins: Maui.Style.space.tiny
                                            spacing: Maui.Style.space.tiny

                                            Label
                                            {
                                                width: parent.width
                                                text: modelData.text
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                            }

                                            Label
                                            {
                                                width: parent.width
                                                visible: parent.parent.height > Maui.Style.rowHeight
                                                text: Qt.formatTime(modelData.startTime, "HH:mm")
                                                elide: Text.ElideRight
                                            }
                                        }

                                        MouseArea
                                        {
                                            anchors.fill: parent
                                            onClicked: control.incidenceActivated(modelData.incidencePtr, modelData.startTime)
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea
                        {
                            anchors.fill: parent
                            z: -1
                            acceptedButtons: Qt.LeftButton
                            onDoubleClicked: function(mouse)
                            {
                                const minutes = Math.floor(mouse.y / control.hourHeight * 4) * 15
                                const date = new Date(dayColumn.dayDate.getFullYear(), dayColumn.dayDate.getMonth(), dayColumn.dayDate.getDate(), Math.floor(minutes / 60), minutes % 60)
                                control.timeSlotActivated(date)
                            }
                        }
                    }
                }
            }
        }
    }
}
