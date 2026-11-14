import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.mauikit.controls as Maui
import org.mauikit.calendar 1.0 as MauiCalendar

/**
 * @inherit QtQuick.Controls.Page
 * @brief A control for picking a time in the format of hour and minutes.
 * 
 * @image html timepicker.png
 * 
 * @code
 *  MauiCalendar.TimePicker
 * {
 *    id: _view
 *    anchors.fill: parent
 *    onAccepted: (time) => console.log("Time Picked, ", time)
 * }
 * @endcode
 */
Page
{
    id:  control
    
    /**
     * @brief
     */
    readonly property date startTime : new Date()
    
    /**
     * @brief
     */
    property int selectedMinute: selectedTime.getMinutes()
    
    /**
     * @brief
     */
    property int selectedHour: selectedTime.getHours()
    
    /**
     * @brief
     */
    property int timeZoneOffset : 0
    
    /**
     * @brief
     */
    property date selectedTime : startTime
    
    /**
     * @brief
     * @param time
     */
    signal accepted(var time)
    
    background: null
    
    header: ToolBar
    {
        width: parent.width
        background: null
        
        contentItem: RowLayout
        {
            spacing: 0
            
            Item {Layout.fillWidth: true}
            
            Button
            {
                text: i18n("Done")
                onClicked:
                {
                    control.updateSelectedTime(minutesTumbler.currentIndex, hoursTumbler.currentIndex)
                    control.accepted(control.selectedTime)
                }
            }
        }
    }    
    
    contentItem: Item
    {        
        Row
        {
            id: row
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter
            
            spacing: Maui.Style.space.medium
            
            Tumbler
            {
                id: hoursTumbler
                spacing: Maui.Style.space.medium
                
                model: 24
                currentIndex: control.selectedHour
                
                delegate:  Button 
                {
                    font.bold: checked
                    
                    checked : index === Tumbler.tumbler.currentIndex
                    text: formatText(Tumbler.tumbler.count, modelData)
                    opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)
                    
                    onClicked: 
                    {
                        Tumbler.tumbler.currentIndex = modelData
                        control.updateSelectedTime(minutesTumbler.currentIndex, hoursTumbler.currentIndex)
                    }
                    
                    background: Rectangle
                    {
                        visible: checked
                        color: checked ? Maui.Theme.highlightColor : hovered ? Maui.Theme.focusColor : "transparent"
                        radius: Maui.Style.radiusV
                    }
                }
                
            }
            
            Tumbler 
            {
                id: minutesTumbler
                model: 60
                spacing: Maui.Style.space.medium
                
                currentIndex: control.selectedMinute
                                
                delegate:  Button 
                {
                    font.bold: checked
                    checked : index === Tumbler.tumbler.currentIndex
                    text: formatText(Tumbler.tumbler.count, modelData)
                    opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)
                    
                    onClicked: 
                    {
                        Tumbler.tumbler.currentIndex = modelData
                        control.updateSelectedTime(minutesTumbler.currentIndex, hoursTumbler.currentIndex)
                    }
                    
                    background: Rectangle
                    {
                        visible: checked
                        color: checked ? Maui.Theme.highlightColor : hovered ? Maui.Theme.focusColor : "transparent"
                        radius: Maui.Style.radiusV
                    }
                }                    
            }
        } 
    }
    
    /**
     * @brief
     * @param count
     * @param modeldata
     */
    function formatText(count : int, modelData : int) : string
    {
        var data = count === 12 ? modelData + 1 : modelData;
        return data.toString().length < 2 ? "0" + data : data;
    }
    
    /**
     * @brief
     * @param minute
     * @param hour
     */
    function updateSelectedTime(minute, hour)
    {
        control.selectedMinute = minute
        control.selectedHour = hour
        
        var newdate = new Date()
        
        newdate.setHours(control.selectedHour)
        newdate.setMinutes(control.selectedMinute) 
        
        control.selectedTime = newdate
        
        console.log("UPDATING TIME", control.selectedTime.getHours(), control.selectedTime.getMinutes(), hour, minute, control.selectedTime.toLocaleTimeString())
    }   
    
}
