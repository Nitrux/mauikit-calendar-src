// SPDX-FileCopyrightText: 2021 Claudio Cambra <claudio.cambra@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "timezonelistmodel.h"
#include <KLocalizedString>
#include <QByteArray>
#include <QDebug>
#include <QMetaEnum>
#include <QTimeZone>

TimeZoneListModel::TimeZoneListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Reimplementation of IncidenceEditorPage's KTimeZoneComboBox
    // Read all system time zones
    const QList<QByteArray> lstTimeZoneIds = QTimeZone::availableTimeZoneIds();
    m_timeZones.reserve(lstTimeZoneIds.count());

    std::copy(lstTimeZoneIds.begin(), lstTimeZoneIds.end(), std::back_inserter(m_timeZones));
    std::sort(m_timeZones.begin(), m_timeZones.end()); // clazy:exclude=detaching-member

    // Prepend Local, UTC and Floating, for convenience.
    m_timeZones.removeAll("UTC");
    m_timeZones.removeAll("Floating");
    m_timeZones.prepend("UTC");
    m_timeZones.prepend("Floating");
    const auto localTimeZone = QTimeZone::systemTimeZoneId();
    if (!localTimeZone.isEmpty() && localTimeZone != "UTC" && localTimeZone != "Floating") {
        m_timeZones.removeAll(localTimeZone);
        m_timeZones.prepend(localTimeZone);
    }
}

QVariant TimeZoneListModel::data(const QModelIndex &idx, int role) const
{
    if (!hasIndex(idx.row(), idx.column())) {
        return {};
    }
    auto timeZone = m_timeZones[idx.row()];
    switch (role) {
    case Qt::DisplayRole:
        if (timeZone == "Floating") return i18n("Auto");
        return timeZone.isEmpty() ? QString() : i18n(timeZone.replace('_', ' ').constData());
    case IdRole:
        return timeZone;
    default:
        qWarning() << "Unknown role for timezone:" << QMetaEnum::fromType<Roles>().valueToKey(role);
        return {};
    }
}

QHash<int, QByteArray> TimeZoneListModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {IdRole, QByteArrayLiteral("id")},
    };
}

int TimeZoneListModel::rowCount(const QModelIndex &) const
{
    return m_timeZones.length();
}

int TimeZoneListModel::getTimeZoneRow(const QByteArray &timeZone)
{
    for (int i = 0; i < rowCount(); i++) {
        QModelIndex idx = index(i, 0);
        QVariant data = idx.data(IdRole).toByteArray();

        if (data == timeZone)
            return i;
    }

    return 0;
}
