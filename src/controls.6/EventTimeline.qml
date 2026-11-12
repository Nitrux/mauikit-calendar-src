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
                        Layout.preferredHeight: Math.max(dayHeaderLabel.implicitHeight, addEventButton.implicitHeight)
                                                + Maui.Style.space.tiny * 2

                        Label
                        {
                            id: dayHeaderLabel
                            anchors.fill: parent
                            anchors.rightMargin: addEventButton.implicitWidth
                            readonly property date headerDate: new Date(control.startDate.getFullYear(), control.startDate.getMonth(), control.startDate.getDate() + index)
                            text: Qt.formatDate(headerDate, "ddd") + "\n" + Qt.formatDate(headerDate, "d")
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            background: Rectangle
                            {
                                visible: Qt.formatDate(dayHeaderLabel.headerDate, "yyyyMMdd") === Qt.formatDate(new Date(), "yyyyMMdd")
                                color: Qt.alpha(Maui.Theme.highlightColor, 0.16)
                                radius: Maui.Style.radiusV
                            }
                        }

                        ToolButton
                        {
                            id: addEventButton
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            icon.name: "list-add"
                            ToolTip.visible: hovered
                            ToolTip.text: i18n("Add event")
                            onClicked: control.dateActivated(new Date(control.startDate.getFullYear(), control.startDate.getMonth(), control.startDate.getDate() + index))
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
                    Layout.preferredHeight: 24 * control.hourHeight

                    Repeater
                    {
                        model: 24

                        delegate: Label
                        {
                            required property int index
                            y: index * control.hourHeight - implicitHeight / 2
                            width: parent.width
                            text: Qt.formatTime(new Date(2000, 0, 1, index), "HH:mm")
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
