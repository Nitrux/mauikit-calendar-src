// SPDX-FileCopyrightText: 2018 Christian Mollekopf, <mollekopf@kolabsys.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import "dateutils.js" as DateUtils

/**
 *  QtQuick.Row
 *  Lays out one delegate for each visible day label.
 *
 * startDate identifies the first day, daysToShow controls the number of labels,
 * and dayWidth fixes the width supplied to each delegate loader. Each delegate
 * receives a day property containing its represented date.
 */
Row
{
    id: root
    property date startDate
    property int dayWidth
    property int daysToShow
    property Component delegate

    //    height: childrenRect.height

    spacing: 0
    Repeater
    {
        model: root.daysToShow
        delegate: Loader
        {
            width: root.dayWidth
            property date day: DateUtils.addDaysToDate(root.startDate, modelData)
            sourceComponent: root.delegate
        }
    }
}

