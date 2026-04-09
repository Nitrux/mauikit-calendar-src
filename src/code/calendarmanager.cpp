// SPDX-FileCopyrightText: 2021 Carl Schwan <carlschwan@kde.org>
// SPDX-FileCopyrightText: 2021 Claudio Cambra <claudio.cambra@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "calendarmanager.h"
#include "incidencewrapper.h"

#include <KCalendarCore/Event>
#include <KCalendarCore/FileStorage>
#include <KCalendarCore/Journal>
#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/Todo>
#include <KLocalizedString>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTimeZone>

Q_GLOBAL_STATIC(CalendarManager, calendarManagerGlobalInstance)

CalendarManager *CalendarManager::instance()
{
    return calendarManagerGlobalInstance;
}

CalendarManager::CalendarManager(QObject *parent)
    : QObject(parent)
{
    m_calendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString filePath = dataDir + QStringLiteral("/default.ics");

    m_storage = new KCalendarCore::FileStorage(m_calendar, filePath);
    if (!m_storage->open()) {
        qInfo() << "Calendar file not found, will create on first save:" << filePath;
    } else if (!m_storage->load()) {
        qWarning() << "Failed to load calendar from" << filePath;
    }

    m_collections = new QStandardItemModel(this);
    m_collections->setItemRoleNames({{Qt::DisplayRole, "display"}, {Qt::CheckStateRole, "checkState"}});
    auto personalItem = new QStandardItem(i18n("Personal"));
    personalItem->setCheckable(true);
    personalItem->setCheckState(Qt::Checked);
    m_collections->appendRow(personalItem);

    // CalendarManager emits calendarChanged() explicitly after every mutation.
}

CalendarManager::~CalendarManager()
{
    save();
}

bool CalendarManager::loading() const
{
    return false;
}

QAbstractItemModel *CalendarManager::collections()
{
    return m_collections;
}

KCalendarCore::MemoryCalendar::Ptr CalendarManager::calendar() const
{
    return m_calendar;
}

KCalendarCore::Incidence::List CalendarManager::childIncidences(const QString &uid) const
{
    KCalendarCore::Incidence::List children;
    const auto all = m_calendar->incidences();
    for (const auto &inc : all) {
        if (inc->relatedTo() == uid) {
            children.append(inc);
        }
    }
    return children;
}

void CalendarManager::save()
{
    if (!m_storage) {
        return;
    }
    if (!m_storage->save()) {
        qWarning() << "Failed to save calendar";
    }
}

void CalendarManager::addIncidence(IncidenceWrapper *incidenceWrapper)
{
    if (!incidenceWrapper || !incidenceWrapper->incidencePtr()) {
        return;
    }
    KCalendarCore::Incidence::Ptr incidence(incidenceWrapper->incidencePtr()->clone());
    m_calendar->addIncidence(incidence);
    save();
}

void CalendarManager::editIncidence(IncidenceWrapper *incidenceWrapper)
{
    if (!incidenceWrapper || !incidenceWrapper->incidencePtr()) {
        return;
    }
    const QString uid = incidenceWrapper->incidencePtr()->uid();
    const auto existing = m_calendar->incidence(uid);
    if (!existing) {
        qWarning() << "Cannot edit: incidence not found:" << uid;
        return;
    }
    m_calendar->deleteIncidence(existing);
    KCalendarCore::Incidence::Ptr updated(incidenceWrapper->incidencePtr()->clone());
    m_calendar->addIncidence(updated);
    save();
}

void CalendarManager::deleteIncidence(KCalendarCore::Incidence::Ptr incidence, bool deleteChildren)
{
    if (!incidence) {
        return;
    }
    if (deleteChildren) {
        const auto children = childIncidences(incidence->uid());
        for (const auto &child : children) {
            deleteIncidence(child, true);
        }
    }
    m_calendar->deleteIncidence(incidence);
    save();
}

bool CalendarManager::hasChildren(KCalendarCore::Incidence::Ptr incidence)
{
    return !childIncidences(incidence->uid()).isEmpty();
}

QVariant CalendarManager::getIncidenceSubclassed(KCalendarCore::Incidence::Ptr incidencePtr)
{
    switch (incidencePtr->type()) {
    case KCalendarCore::IncidenceBase::TypeEvent:
        return QVariant::fromValue(m_calendar->event(incidencePtr->instanceIdentifier()));
    case KCalendarCore::IncidenceBase::TypeTodo:
        return QVariant::fromValue(m_calendar->todo(incidencePtr->instanceIdentifier()));
    case KCalendarCore::IncidenceBase::TypeJournal:
        return QVariant::fromValue(m_calendar->journal(incidencePtr->instanceIdentifier()));
    default:
        return QVariant::fromValue(incidencePtr);
    }
}
