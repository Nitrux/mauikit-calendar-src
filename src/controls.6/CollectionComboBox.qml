// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.0-or-later

import QtQuick
import QtQuick.Controls 
import org.mauikit.calendar 1.0 as MauiCalendar
import org.mauikit.controls as Maui

/**
 *  Selects a calendar collection from a CollectionComboBoxModel.
 * The collection displayed can be filtered using the  mimeTypeFilter
 * and @p accessRightsFilter properties.
 */
ComboBox
{
    id: comboBox

    /**
     * This property holds the id of the default collection, that is the
     * collection that will be selected by default.
     * @property int defaultCollectionId
     */
    property alias defaultCollectionId: collectionComboBoxModel.defaultCollectionId

    /**
     * This property holds the mime types of the collection that should be
     * displayed.
     *
     * @property list<string> mimeTypeFilter
     * @code{.qml}
     * import org.mauikit.calendar 1.0 as MauiCalendar
     * 
     * MauiCalendar.CollectionComboBoxModel {
     *     mimeTypeFilter: [MauiCalendar.MimeTypes.address, MauiCalendar.MimeTypes.contactGroup]
     * }
     * @endcode
     */
    property alias mimeTypeFilter: collectionComboBoxModel.mimeTypeFilter

    /**
     * This property holds the access right of the collection that should be
     * displayed.
     *
     * @property Calendar::Collection::Rights rights
     * @code{.qml}
     * import org.mauikit.calendar 1.0 as MauiCalendar
     * 
     * MauiCalendar.CollectionComboBoxModel {
     *     accessRightsFilter: MauiCalendar.Collection.CanCreateItem
     * }
     * @endcode
     */
    property alias accessRightsFilter: collectionComboBoxModel.accessRightsFilter

    signal userSelectedCollection(var collection)

    currentIndex: 0
    onActivated: if (index > -1) {
        const selectedModelIndex = collectionComboBoxModel.index(currentIndex, 0);
        const selectedCollection = collectionComboBoxModel.data(selectedModelIndex, MauiCalendar.Collection.CollectionRole);
        userSelectedCollection(selectedCollection);
    }

    textRole: "display"
    valueRole: "collectionId"

    indicator: Rectangle {
        id: indicatorDot

        // Make sure to check the currentValue property directly or risk listening to something that won't necessarily emit a changed() signal'
        readonly property var selectedModelIndex: comboBox.currentValue > -1 ? comboBox.model.index(comboBox.currentIndex, 0) : null
        readonly property var selectedCollectionColor: comboBox.currentValue > -1 ? comboBox.model.data(selectedModelIndex, MauiCalendar.Collection.CollectionColorRole) : null

        implicitHeight: comboBox.implicitHeight * 0.4
        implicitWidth: implicitHeight

        x: comboBox.mirrored ? comboBox.leftPadding : comboBox.width - (comboBox.leftPadding * 3) - Maui.Style.iconSizes.medium
        y: comboBox.topPadding + (comboBox.availableHeight - height) / 2

        radius: width * 0.5
        color: selectedCollectionColor
    }

    model: MauiCalendar.CollectionComboBoxModel {
        id: collectionComboBoxModel
        onCurrentIndexChanged: comboBox.currentIndex = currentIndex
    }

    delegate: MenuItem
    {
        text: display
        icon.name: decoration
        indicator: Rectangle {
            anchors.margins: Maui.Style.space.medium
            width: height
            radius: width * 0.5
            color: model.collectionColor
        }
    }

    popup.z: 1000
}
