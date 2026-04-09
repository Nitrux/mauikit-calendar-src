// SPDX-FileCopyrightText: 2021 Carl Schwan <carlschwan@kde.org>
// SPDX-FileCopyrightText: 2021 Claudio Cambra <claudio.cambra@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QAbstractItemModel>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QStandardItemModel>

#include <KCalendarCore/FileStorage>
#include <KCalendarCore/MemoryCalendar>

#include "calendar_export.h"

#include "incidencewrapper.h"

class CALENDAR_EXPORT CalendarManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QAbstractItemModel *collections READ collections CONSTANT)

public:
    static CalendarManager *instance();
    static CalendarManager *create(QQmlEngine *engine, QJSEngine *)
    {
        engine->setObjectOwnership(instance(), QQmlEngine::CppOwnership);
        return instance();
    }
    explicit CalendarManager(QObject *parent = nullptr);
    ~CalendarManager() override;

    bool loading() const;
    QAbstractItemModel *collections();
    KCalendarCore::MemoryCalendar::Ptr calendar() const;
    KCalendarCore::Incidence::List childIncidences(const QString &uid) const;

    Q_INVOKABLE void save();
    Q_INVOKABLE void addIncidence(IncidenceWrapper *incidenceWrapper);
    Q_INVOKABLE void editIncidence(IncidenceWrapper *incidenceWrapper);
    Q_INVOKABLE void deleteIncidence(KCalendarCore::Incidence::Ptr incidence, bool deleteChildren = false);
    Q_INVOKABLE bool hasChildren(KCalendarCore::Incidence::Ptr incidence);
    Q_INVOKABLE QVariant getIncidenceSubclassed(KCalendarCore::Incidence::Ptr incidencePtr);

Q_SIGNALS:
    void loadingChanged();
    void calendarChanged();

private:
    KCalendarCore::MemoryCalendar::Ptr m_calendar;
    KCalendarCore::FileStorage *m_storage = nullptr;
    QStandardItemModel *m_collections = nullptr;
};

