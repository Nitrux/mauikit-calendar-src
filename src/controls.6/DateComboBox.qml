import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.mauikit.controls as Maui
import org.mauikit.calendar 1.0 as MauiCalendar

/**
 *  QtQuick.Controls.ComboBox
 *  Opens a DatePicker and displays its selected date.
 *
 * selectedDay, selectedMonth, selectedYear, and selectedDate mirror the picker.
 * datePicked is emitted after the user accepts a date.
 */
ComboBox
{
    id:  control
    enabled: true

    property alias selectedMonth : _picker.selectedMonth
    property alias selectedYear: _picker.selectedYear
    property alias selectedDay : _picker.selectedDay

    property alias selectedDate : _picker.selectedDate

    displayText: selectedDate.toLocaleDateString()

    font.bold: true
    font.weight: Font.Bold
    font.family: "Monospace"
    Maui.Controls.iconName: "view-calendar"

    signal datePicked(var date)

    popupContent: MauiCalendar.DatePicker
    {
        id: _picker
        implicitHeight: 300
        background: null

        onAccepted:
        {
            control.datePicked(date)
            control.accepted()
            control.popup.close()
        }
    }
}
