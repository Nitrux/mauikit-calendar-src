// Copyright (c) 2018 Michael Bohlender <michael.bohlender@kdemail.net>
// Copyright (c) 2018 Christian Mollekopf <mollekopf@kolabsys.com>
// Copyright (c) 2018 Rémi Nicole <minijackson@riseup.net>
// Copyright (c) 2021 Carl Schwan <carlschwan@kde.org>
// Copyright (c) 2021 Claudio Cambra <claudio.cambra@gmail.com>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "incidenceoccurrencemodel.h"

#include "../calendarmanager.h"
#include "../filter.h"
#include <KCalendarCore/OccurrenceIterator>
#include <KLocalizedString>
#include <QMetaEnum>

IncidenceOccurrenceModel::IncidenceOccurrenceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_resetThrottlingTimer.setSingleShot(true);
    QObject::connect(&m_resetThrottlingTimer, &QTimer::timeout, this, &IncidenceOccurrenceModel::resetFromSource);

    // Refresh whenever the calendar changes
    connect(CalendarManager::instance(), &CalendarManager::calendarChanged,
            this, &IncidenceOccurrenceModel::scheduleReset);
}

void IncidenceOccurrenceModel::setStart(const QDate &start)
{
    if (start == mStart) {
        return;
    }
    mStart = start;
    Q_EMIT startChanged();
    mEnd = mStart.addDays(mLength);
    scheduleReset();
}

QDate IncidenceOccurrenceModel::start() const
{
    return mStart;
}

void IncidenceOccurrenceModel::setLength(int length)
{
    if (mLength == length) {
        return;
    }
    mLength = length;
    Q_EMIT lengthChanged();
    mEnd = mStart.addDays(mLength);
    scheduleReset();
}

int IncidenceOccurrenceModel::length() const
{
    return mLength;
}

Filter *IncidenceOccurrenceModel::filter() const
{
    return mFilter;
}

void IncidenceOccurrenceModel::setFilter(Filter *filter)
{
    mFilter = filter;
    Q_EMIT filterChanged();
    scheduleReset();
}

bool IncidenceOccurrenceModel::loading() const
{
    return m_loading;
}

void IncidenceOccurrenceModel::setLoading(const bool loading)
{
    if (loading == m_loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

int IncidenceOccurrenceModel::resetThrottleInterval() const
{
    return m_resetThrottleInterval;
}

void IncidenceOccurrenceModel::setResetThrottleInterval(const int resetThrottleInterval)
{
    if (resetThrottleInterval == m_resetThrottleInterval) {
        return;
    }
    m_resetThrottleInterval = resetThrottleInterval;
    Q_EMIT resetThrottleIntervalChanged();
}

void IncidenceOccurrenceModel::scheduleReset()
{
    if (!m_resetThrottlingTimer.isActive()) {
        m_resetThrottlingTimer.start(m_resetThrottleInterval);
    }
}

void IncidenceOccurrenceModel::resetFromSource()
{
    const auto calendar = CalendarManager::instance()->calendar();
    if (!calendar) {
        return;
    }

    if (m_resetThrottlingTimer.isActive()) {
        scheduleReset();
        return;
    }

    setLoading(true);
    beginResetModel();
    m_incidences.clear();

    KCalendarCore::OccurrenceIterator occurrenceIterator(*calendar, QDateTime(mStart, {0, 0, 0}), QDateTime(mEnd, {12, 59, 59}));

    while (occurrenceIterator.hasNext()) {
        occurrenceIterator.next();
        const auto incidence = occurrenceIterator.incidence();

        if (!incidencePassesFilter(incidence)) {
            continue;
        }

        const auto occurrenceStartEnd = incidenceOccurrenceStartEnd(occurrenceIterator.occurrenceStartDate(), incidence);
        const Occurrence occurrence{
            occurrenceStartEnd.first,
            occurrenceStartEnd.second,
            incidence,
            QColor(),
            0,
            incidence->allDay(),
        };
        m_incidences.append(occurrence);
    }

    endResetModel();
    setLoading(false);
}

int IncidenceOccurrenceModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_incidences.size();
    }
    return 0;
}

QVariant IncidenceOccurrenceModel::data(const QModelIndex &idx, int role) const
{
    if (!hasIndex(idx.row(), idx.column())) {
        return {};
    }

    const auto occurrence = m_incidences.at(idx.row());
    const auto incidence = occurrence.incidence;

    switch (role) {
    case Summary:
        return incidence->summary();
    case Description:
        return incidence->description();
    case Location:
        return incidence->location();
    case StartTime:
        return occurrence.start;
    case EndTime:
        return occurrence.end;
    case Duration: {
        const KCalendarCore::Duration duration(occurrence.start, occurrence.end);
        return QVariant::fromValue(duration);
    }
    case DurationString: {
        const KCalendarCore::Duration duration(occurrence.start, occurrence.end);
        if (duration.asSeconds() == 0) {
            return QString();
        }
        return m_format.formatSpelloutDuration(duration.asSeconds() * 1000);
    }
    case Recurs:
        return incidence->recurs();
    case HasReminders:
        return incidence->alarms().length() > 0;
    case Priority:
        return incidence->priority();
    case Color:
        return occurrence.color;
    case CollectionId:
        return occurrence.collectionId;
    case AllDay:
        return occurrence.allDay;
    case TodoCompleted: {
        if (incidence->type() != KCalendarCore::IncidenceBase::TypeTodo) {
            return false;
        }
        auto todo = incidence.staticCast<KCalendarCore::Todo>();
        return todo->isCompleted();
    }
    case IsOverdue: {
        if (incidence->type() != KCalendarCore::IncidenceBase::TypeTodo) {
            return false;
        }
        auto todo = incidence.staticCast<KCalendarCore::Todo>();
        return todo->isOverdue();
    }
    case IsReadOnly:
        return false;
    case IncidenceId:
        return incidence->uid();
    case IncidenceType:
        return incidence->type();
    case IncidenceTypeStr:
        return incidence->type() == KCalendarCore::Incidence::TypeTodo ? i18n("Task") : i18n(incidence->typeStr().constData());
    case IncidenceTypeIcon:
        return incidence->iconName();
    case IncidencePtr:
        return QVariant::fromValue(incidence);
    case IncidenceOccurrence:
        return QVariant::fromValue(occurrence);
    default:
        qWarning() << "Unknown role for occurrence:" << QMetaEnum::fromType<Roles>().valueToKey(role);
        return {};
    }
}

QHash<int, QByteArray> IncidenceOccurrenceModel::roleNames() const
{
    return {
        {Summary, "summary"},
        {Description, "description"},
        {Location, "location"},
        {StartTime, "startTime"},
        {EndTime, "endTime"},
        {Duration, "duration"},
        {DurationString, "durationString"},
        {Recurs, "recurs"},
        {HasReminders, "hasReminders"},
        {Priority, "priority"},
        {Color, "color"},
        {CollectionId, "collectionId"},
        {AllDay, "allDay"},
        {TodoCompleted, "todoCompleted"},
        {IsOverdue, "isOverdue"},
        {IsReadOnly, "isReadOnly"},
        {IncidenceId, "incidenceId"},
        {IncidenceType, "incidenceType"},
        {IncidenceTypeStr, "incidenceTypeStr"},
        {IncidenceTypeIcon, "incidenceTypeIcon"},
        {IncidencePtr, "incidencePtr"},
        {IncidenceOccurrence, "incidenceOccurrence"},
    };
}

std::pair<QDateTime, QDateTime> IncidenceOccurrenceModel::incidenceOccurrenceStartEnd(const QDateTime &ocStart, const KCalendarCore::Incidence::Ptr &incidence)
{
    auto start = ocStart;
    const auto end = incidence->endDateForStart(start);

    if (incidence->type() == KCalendarCore::Incidence::IncidenceType::TypeTodo) {
        KCalendarCore::Todo::Ptr todo = incidence.staticCast<KCalendarCore::Todo>();
        if (!start.isValid()) {
            start = todo->dtDue();
        }
    }
    return {start, end};
}

uint IncidenceOccurrenceModel::incidenceOccurrenceHash(const QDateTime &ocStart, const QDateTime &ocEnd, const QString &incidenceUid)
{
    return qHash(ocStart) ^ qHash(ocEnd) ^ qHash(incidenceUid);
}

bool IncidenceOccurrenceModel::incidencePassesFilter(const KCalendarCore::Incidence::Ptr &incidence)
{
    if (!mFilter) {
        return true;
    }
    if (!mFilter->name().isEmpty() && !incidence->summary().contains(mFilter->name(), Qt::CaseInsensitive)) {
        return false;
    }
    return true;
}
