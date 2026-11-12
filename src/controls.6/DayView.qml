import QtQuick
import org.mauikit.controls as Maui
import org.mauikit.calendar 1.0 as MauiCalendar

Item
{
    id: control

    property date selectedDate: new Date()
    property var filter: MauiCalendar.Filter

    signal incidenceActivated(var incidence, var occurrence)
    signal timeSlotActivated(var date)
    signal dateActivated(var date)

    EventTimeline
    {
        anchors.fill: parent
        startDate: control.selectedDate
        dayCount: 1
        filter: control.filter
        onIncidenceActivated: function(incidence, occurrence) { control.incidenceActivated(incidence, occurrence) }
        onTimeSlotActivated: function(date) { control.timeSlotActivated(date) }
        onDateActivated: function(date) { control.dateActivated(date) }
    }
}
