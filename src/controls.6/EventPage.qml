import QtQuick
import QtQml
import QtQuick.Controls
import QtQuick.Layouts
import org.mauikit.controls as Maui
import org.mauikit.calendar as Cal

/**
 * @inherit QtQuick.Controls.Pane
 * @brief A view field for creating a new calendar event.
 */
Pane
{
    id: control

    implicitHeight: _layout.implicitHeight + topPadding + bottomPadding

    padding: 0
    background: null

    /**
     * @brief
     */
    property alias incidence : incidenceWrapper

    Cal.IncidenceWrapper
    {
        id: incidenceWrapper

        onIncidenceStartChanged:
        {
            incidenceStartDateCombo.selectedDate = control.incidence.incidenceStart;
            incidenceStartTimeCombo.selectedTime = incidenceWrapper.incidenceStart;
            incidenceStartDateCombo.displayText = incidenceWrapper.incidenceStartDateDisplay;
            incidenceStartTimeCombo.displayText = incidenceWrapper.incidenceStartTimeDisplay;
        }

        onIncidenceEndChanged:
        {
            incidenceEndDateCombo.selectedDate = control.incidence.incidenceEnd;
            incidenceEndTimeCombo.selectedTime = incidenceWrapper.incidenceEnd;
            incidenceEndDateCombo.displayText = incidenceWrapper.incidenceEndDateDisplay;
            incidenceEndTimeCombo.displayText = incidenceWrapper.incidenceEndTimeDisplay;
        }
    }

    contentItem: Maui.ScrollColumn
    {
        id: _layout

        Maui.SectionGroup
        {
            title: i18n("Info")

            Maui.SectionItem
            {
                label1.text: i18n("Description")

                TextField
                {
                    Layout.fillWidth: true
                    height: 80
                    text: incidenceWrapper.summary
                    placeholderText: i18n(`Add a title for your ${incidenceWrapper.incidenceTypeStr.toLowerCase()}`)
                    onTextChanged: incidenceWrapper.summary = text

                    onAccepted : incidenceWrapper.summary = text
                }
            }

            Maui.SectionItem
            {
                label1.text: i18n("Calendar")

                ComboBox
                {
                    id: calendarCombo
                    Layout.fillWidth: true
                    textRole: "display"

                    model: Cal.CollectionComboBoxModel
                    {
                        id: collectionComboBoxModel
                        onCurrentIndexChanged: calendarCombo.currentIndex = currentIndex
                        defaultCollectionId: incidenceWrapper.collectionId
                    }

                    currentIndex: 0
                    onCurrentIndexChanged: if (currentIndex !== -1) {
                        incidenceWrapper.collectionId = currentIndex
                    }
                }
            }
        }


        Maui.SectionGroup
        {
            title: i18n("Time")

            Maui.FlexSectionItem
            {
                label1.text: i18n("All day")
                enabled: !isNaN(incidenceWrapper.incidenceStart.getTime()) || !isNaN(incidenceWrapper.incidenceEnd.getTime())

                Switch
                {
                    id: allDayCheckBox
                    checked: incidenceWrapper.allDay
                    onToggled:
                    {
                        if (!checked)
                        {
                            incidenceWrapper.setIncidenceTimeToNearestQuarterHour();
                        }
                        incidenceWrapper.allDay = checked;
                    }
                }
            }

            Maui.SectionItem
            {
                label1.text: i18n("Start")

                Cal.DateComboBox
                {
                    id: incidenceStartDateCombo
                    Layout.fillWidth: true
                    displayText: incidenceWrapper.incidenceStartDateDisplay
                    selectedDate: incidenceWrapper.incidenceStart
                    onDatePicked:
                    {
                        incidenceWrapper.setIncidenceStartDate(date.getDate(), date.getMonth(), date.getYear())
                    }
                }

                Cal.TimeComboBox
                {
                    id: incidenceStartTimeCombo
                    Layout.fillWidth: true
                    visible: !allDayCheckBox.checked

                    timeZoneOffset: incidenceWrapper.startTimeZoneUTCOffsetMins
                    displayText: incidenceWrapper.incidenceEndTimeDisplay
                    selectedTime: incidenceWrapper.incidenceStart
                    onTimePicked:
                    {
                        incidenceWrapper.setIncidenceStartTime(time.getHours(), time.getMinutes())
                    }
                }
            }

            Maui.SectionItem
            {
                label1.text: i18n("End")

                Cal.DateComboBox
                {
                    id: incidenceEndDateCombo
                    Layout.fillWidth: true
                    displayText: incidenceWrapper.incidenceStartDateDisplay
                    selectedDate: incidenceWrapper.incidenceStart
                    onDatePicked:
                    {
                        incidenceWrapper.setIncidenceEndDate(date.getDate(), date.getMonth(), date.getYear())
                    }
                }

                Cal.TimeComboBox
                {
                    id: incidenceEndTimeCombo
                    Layout.fillWidth: true
                    visible: !allDayCheckBox.checked
                    timeZoneOffset: incidenceWrapper.endTimeZoneUTCOffsetMins
                    displayText: incidenceWrapper.incidenceEndTimeDisplay
                    selectedTime: incidenceWrapper.incidenceEnd
                    onTimePicked:
                    {
                        incidenceWrapper.setIncidenceEndTime(hours, minutes)
                    }
                }
            }

            Maui.SectionItem
            {
                label1.text: i18n("TimeZone")

                ComboBox
                {
                    Layout.fillWidth: true
                    model: Cal.TimeZoneListModel
                    {
                        id: timeZonesModel
                    }

                    textRole: "display"
                    valueRole: "id"
                    currentIndex: model ? timeZonesModel.getTimeZoneRow(incidenceWrapper.timeZone) : -1

                    onActivated: incidenceWrapper.timeZone = currentValue
                }
            }
        }
    }
}
