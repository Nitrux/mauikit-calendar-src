// Copyright (C) 2018 Michael Bohlender, <bohlender@kolabsys.com>
// Copyright (C) 2018 Christian Mollekopf, <mollekopf@kolabsys.com>
// SPDX-FileCopyrightText: 2021 Claudio Cambra <claudio.cambra@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.mauikit.controls as Maui

import org.mauikit.calendar 1.0 as MauiCalendar
import "dateutils.js" as DateUtils
import "labelutils.js" as LabelUtils

/**
 *  QtQuick.Controls.Pane
 *  Displays a configurable grid of calendar days and their incidences.
 *
 * The grid begins at startDate and contains daysToShow cells arranged in
 * daysPerRow columns. Applications provide dayHeaderDelegate and optionally
 * weekHeaderDelegate, and receive dateClicked or dateDoubleClicked when a day
 * is activated.
 */
QQC2.Pane
{
    id: root

    property var openOccurrence
    property var model

    MauiCalendar.IncidenceOccurrenceModel
    {
        id: monthOccurrences
        start: root.startDate
        length: root.daysToShow
        filter: MauiCalendar.Filter
    }

    MauiCalendar.MultiDayIncidenceModel
    {
        id: multiDayLayout
        model: monthOccurrences
        periodLength: root.daysPerRow
        filters: MauiCalendar.MultiDayIncidenceModel.AllDayOnly | MauiCalendar.MultiDayIncidenceModel.MultiDayOnly
    }

    property int daysToShow: daysPerRow * 6
    property int daysPerRow: 7
    property double weekHeaderWidth: root.showWeekNumbers ? Maui.Style.units.gridUnit * 1.5 : 0
    property date currentDate
    property date selectedDate

    property int currentDay: currentDate ? currentDate.getDate() : null
    property int currentMonth: currentDate ? currentDate.getMonth() : null
    property int currentYear: currentDate ? currentDate.getFullYear() : null

    property date startDate
    
    property bool paintGrid: true
    property bool showDayIndicator: true

    property Component dayHeaderDelegate
    property Component weekHeaderDelegate

    property int month
    property alias bgLoader: backgroundLoader.item
    property bool isCurrentView: true
    property bool dragDropEnabled: true
    property bool showWeekNumbers : false

    //Internal
    property int numberOfLinesShown: 0
    property int numberOfRows: (daysToShow / daysPerRow)

    property int dayWidth: (root.showWeekNumbers ?
                                ((width - weekHeaderWidth) / daysPerRow) - spacing : // No spacing on right, spacing in between weekheader and monthgrid
                                (width -  rightPadding  - leftPadding - weekHeaderWidth - (spacing * (daysPerRow - 1))) / daysPerRow) // No spacing on left or right of month grid when no week header

    property int dayHeight: ((height - topPadding - bottomPadding - bgLoader.dayLabelsBar.height) / numberOfRows) - spacing

    readonly property bool isDark: CalendarUiUtils.darkMode
    //    readonly property int mode: MauiCalendar.CalendarApplication.Event

    //    implicitHeight: (numberOfRows > 1 ? Maui.Style.units.gridUnit * 10 * numberOfRows : numberOfLinesShown * Maui.Style.units.gridUnit) + bgLoader.dayLabelsBar.height +topPadding + bottomPadding
    //    height: implicitHeight
    readonly property bool isWide : dayWidth > Maui.Style.units.gridUnit * 5

    padding: Maui.Style.space.medium
    spacing: Maui.Style.space.small
    background: null
    
    signal dateClicked(var date)
    signal dateDoubleClicked(var date)
    signal incidenceClicked(var incidence, var occurrence)
    signal moreEventsClicked(var date)
    
    contentItem: Loader
    {
        id: backgroundLoader
        asynchronous: !root.isCurrentView
        sourceComponent: Column
        {
            id: rootBackgroundColumn
            spacing: root.spacing

            property alias dayLabelsBar: dayLabelsBarComponent
            MauiCalendar.DayLabelsBar
            {
                id: dayLabelsBarComponent
                delegate: root.dayHeaderDelegate
                startDate: root.startDate
                dayWidth: root.dayWidth
                daysToShow: root.daysPerRow
                spacing: root.spacing
                anchors.leftMargin: root.showWeekNumbers ? weekHeaderWidth + root.spacing : 0
                anchors.left: parent.left
                anchors.right: parent.right
            }

            Repeater
            {
                model: multiDayLayout

                //One row => one week
                Item
                {
                    id: weekRow
                    required property int index
                    required property var incidences
                    width: parent.width
                    height: root.dayHeight
                    clip: true

                    RowLayout
                    {
                        width: parent.width
                        height: parent.height
                        spacing: root.spacing

                        Loader
                        {
                            id: weekHeader
                            sourceComponent: root.weekHeaderDelegate
                            property date startDate: DateUtils.addDaysToDate(root.startDate, weekRow.index * 7)
                            Layout.preferredWidth: weekHeaderWidth
                            Layout.fillHeight: true
                            active: root.showWeekNumbers
                            visible: root.showWeekNumbers
                        }

                        Item
                        {
                            id: dayDelegate
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            property date startDate: DateUtils.addDaysToDate(root.startDate, weekRow.index * 7)

                            //Grid
                            Row
                            {
                                spacing: root.spacing
                                height: parent.height

                                Repeater
                                {
                                    id: gridRepeater
                                    model: root.daysPerRow

                                    QQC2.Button
                                    {
                                        id: gridItem
                                        required property int index
                                        Maui.Theme.colorSet: Maui.Theme.View
                                        Maui.Theme.inherit: false

                                        height: root.dayHeight
                                        width: root.dayWidth
                                        clip: true
                                        enabled: root.daysToShow > 1
                                        visible: root.showDayIndicator
                                        padding: Maui.Style.space.small
                                        onClicked: root.dateClicked(gridItem.date)
                                        onDoubleClicked: root.dateDoubleClicked(gridItem.date)

                                        property date gridSquareDate: date
                                        property date date: DateUtils.addDaysToDate(dayDelegate.startDate, gridItem.index)
                                        property int day: date.getDate()
                                        property int month: date.getMonth()
                                        property int year: date.getFullYear()
                                        property bool isToday: day === root.currentDay && month === root.currentMonth && year === root.currentYear
                                        property bool isSelected: root.selectedDate
                                                && day === root.selectedDate.getDate()
                                                && month === root.selectedDate.getMonth()
                                                && year === root.selectedDate.getFullYear()
                                        property bool isCurrentMonth: month === root.month

                                        background: Rectangle
                                        {
                                            color: gridItem.isToday || gridItem.isSelected ? Maui.Theme.highlightColor :
                                                   gridItem.hovered ? Maui.Theme.hoverColor : "transparent"
                                            radius: Maui.Style.radiusV
                                        }

                                        contentItem: Item
                                        {
                                            id: eventCell
                                            clip: true
                                            property int singleDayEventCount: 0

                                            QQC2.Label
                                            {
                                                id: _dateLabel
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                anchors.verticalCenter: root.isWide ? undefined : parent.verticalCenter
                                                anchors.top: root.isWide ? parent.top : undefined
                                                anchors.topMargin: root.isWide ? Maui.Style.space.tiny : 0
                                                visible: root.showDayIndicator
                                                text: gridItem.date.toLocaleDateString(Qt.locale(), gridItem.day == 1 && root.isWide ? "d MMM" : "d")
                                                renderType: Text.QtRendering
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter

                                                color: gridItem.isToday || gridItem.isSelected ? Maui.Theme.highlightedTextColor :
                                                       (!gridItem.isCurrentMonth ? Qt.alpha(Maui.Theme.textColor, 0.5) : Maui.Theme.textColor)
                                                font.bold: root.isWide
                                                font.weight: root.isWide ? Font.Bold : Font.Normal
                                                font.pointSize: root.isWide ? Maui.Style.fontSizes.big : Maui.Style.fontSizes.small
                                            }

                                            Flow
                                            {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: _dateLabel.bottom
                                                anchors.bottom: parent.bottom
                                                anchors.topMargin: Maui.Style.space.small
                                                spacing: Maui.Style.space.tiny
                                                clip: true

                                                Repeater
                                                {
                                                    model: MauiCalendar.IncidenceOccurrenceModel
                                                    {
                                                        start: gridItem.date
                                                        length: 0
                                                        filter: MauiCalendar.Filter
                                                    }

                                                    delegate: QQC2.Button
                                                    {
                                                        required property string summary
                                                        required property bool allDay
                                                        required property var color
                                                        required property var incidencePtr
                                                        required property var startTime
                                                        required property var endTime
                                                        property int singleDayEventIndex: -1
                                                        readonly property bool singleDayEvent: gridItem.isCurrentMonth && !allDay && startTime.toDateString() === gridItem.date.toDateString() && endTime.toDateString() === gridItem.date.toDateString()

                                                        Component.onCompleted:
                                                        {
                                                            if (singleDayEvent)
                                                            {
                                                                singleDayEventIndex = eventCell.singleDayEventCount
                                                                eventCell.singleDayEventCount++
                                                            }
                                                        }
                                                        Component.onDestruction: if (singleDayEvent) eventCell.singleDayEventCount--

                                                        implicitWidth: Math.min(eventLabel.implicitWidth + Maui.Style.space.medium, parent.width)
                                                        implicitHeight: Maui.Style.rowHeight
                                                        padding: Maui.Style.space.tiny
                                                        text: summary
                                                        visible: singleDayEvent && singleDayEventIndex < 2
                                                        onClicked: root.incidenceClicked(incidencePtr, startTime)

                                                        contentItem: QQC2.Label
                                                        {
                                                            id: eventLabel
                                                            text: (allDay ? "" : Qt.formatTime(startTime, "HH:mm") + " ") + summary
                                                            color: Maui.Theme.textColor
                                                            elide: Text.ElideRight
                                                        }

                                                        background: Rectangle
                                                        {
                                                            radius: Maui.Style.radiusV
                                                            color: "transparent"
                                                            border.color: parent.color
                                                            border.width: Maui.Style.units.smallSpacing / 2
                                                        }
                                                    }
                                                }

                                                QQC2.Button
                                                {
                                                    visible: eventCell.singleDayEventCount > 2
                                                    implicitHeight: Maui.Style.rowHeight
                                                    padding: Maui.Style.space.tiny
                                                    text: i18n("+%1 more", eventCell.singleDayEventCount - 2)
                                                    onClicked: root.moreEventsClicked(gridItem.date)
                                                }
                                            }

                                        }

                                    }
                                }
                            }
                        }
                    }

                    Repeater
                    {
                        model: weekRow.incidences

                        delegate: Item
                        {
                            required property var modelData
                            required property int index
                            anchors.fill: weekRow

                            Repeater
                            {
                                model: parent.modelData

                                delegate: QQC2.Button
                                {
                                    id: spanningEvent
                                    required property var modelData
                                    x: root.weekHeaderWidth + root.spacing + Number(modelData.starts) * (root.dayWidth + root.spacing)
                                    y: Maui.Style.rowHeight + parent.index * Maui.Style.rowHeight
                                    width: Number(modelData.duration) * (root.dayWidth + root.spacing) - root.spacing
                                    height: Maui.Style.rowHeight
                                    padding: Maui.Style.space.tiny
                                    text: modelData.text
                                    visible: modelData.incidencePtr !== undefined
                                    onClicked: root.incidenceClicked(modelData.incidencePtr, modelData.startTime)

                                    background: Rectangle
                                    {
                                        radius: Maui.Style.radiusV
                                        color: "transparent"
                                        border.color: modelData.color
                                        border.width: Maui.Style.units.smallSpacing / 2
                                    }

                                    contentItem: QQC2.Label
                                    {
                                        text: spanningEvent.modelData.text
                                        color: Maui.Theme.textColor
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    //    Loader {
    //        id: foregroundLoader
    //        anchors.fill: parent
    //        asynchronous: !root.isCurrentView
    //        sourceComponent: Column {
    //            id: rootForegroundColumn
    //            spacing: root.spacing
    //            anchors {
    //                fill: parent
    //                topMargin: root.bgLoader.dayLabelsBar.height + root.spacing
    //                leftMargin: root.showWeekNumbers ? weekHeaderWidth + root.spacing : 0
    //            }

    //            //Weeks
    //            Repeater {
    //                model: root.model
    //                //One row => one week
    //                Item {
    //                    width: parent.width
    //                    height: root.dayHeight
    //                    clip: true
    //                    RowLayout {
    //                        width: parent.width
    //                        height: parent.height
    //                        spacing: root.spacing
    //                        Item {
    //                            id: dayDelegate
    //                            Layout.fillWidth: true
    //                            Layout.fillHeight: true
    //                            property date startDate: periodStartDate

    //                            ListView {
    //                                id: linesRepeater

    //                                anchors {
    //                                    fill: parent
    //                                    // Offset for date
    //                                    topMargin: root.showDayIndicator ? Kirigami.Units.gridUnit + Kirigami.Units.largeSpacing * 1.5 : 0
    //                                    rightMargin: spacing
    //                                }

    //                                // DO NOT use a ScrollView as a bug causes this to crash randomly.
    //                                // So we instead make the ListView act like a ScrollView on desktop. No crashing now!
    //                                flickableDirection: Flickable.VerticalFlick
    //                                boundsBehavior: Kirigami.Settings.isMobile ? Flickable.DragAndOvershootBounds : Flickable.StopAtBounds
    //                                QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

    //                                clip: true
    //                                spacing: root.listViewSpacing

    //                                DayMouseArea {
    //                                    id: listViewMenu
    //                                    anchors.fill: parent
    //                                    z: -1

    //                                    function useGridSquareDate(type, root, globalPos) {
    //                                        for(var i in root.children) {
    //                                            var child = root.children[i];
    //                                            var localpos = child.mapFromGlobal(globalPos.x, globalPos.y);

    //                                            if(child.contains(localpos) && child.gridSquareDate) {
    //                                                CalendarUiUtils.setUpAdd(type, child.gridSquareDate);
    //                                            } else {
    //                                                useGridSquareDate(type, child, globalPos);
    //                                            }
    //                                        }
    //                                    }

    //                                    onAddNewIncidence: useGridSquareDate(type, applicationWindow().contentItem, this.mapToGlobal(clickX, clickY))
    //                                    onDeselect: CalendarUiUtils.appMain.incidenceInfoDrawer.close()
    //                                }

    //                                model: incidences
    //                                onCountChanged: {
    //                                    root.numberOfLinesShown = count
    //                                }

    //                                delegate: Item {
    //                                    id: line
    //                                    height: Kirigami.Units.gridUnit + Kirigami.Units.smallSpacing

    //                                    //Incidences
    //                                    Repeater {
    //                                        id: incidencesRepeater
    //                                        model: modelData

    //                                        DayGridViewIncidenceDelegate {
    //                                            id: incidenceDelegate
    //                                            dayWidth: root.dayWidth
    //                                            height: line.height
    //                                            parentViewSpacing: root.spacing
    //                                            horizontalSpacing: linesRepeater.spacing
    //                                            openOccurrenceId: root.openOccurrence ? root.openOccurrence.incidenceId : ""
    //                                            isDark: root.isDark
    //                                            dragDropEnabled: root.dragDropEnabled
    //                                        }
    //                                    }
    //                                }
    //                            }
    //                        }
    //                    }
    //                }
    //            }
    //        }
    //    }
}
