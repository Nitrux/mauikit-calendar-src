#include "plugin.h"

#include <QQmlEngine>

#include "infinitecalendarviewmodel.h"
#include "hourlyincidencemodel.h"
#include "incidenceoccurrencemodel.h"
#include "timezonelistmodel.h"
#include "monthmodel.h"

#include "calendarmanager.h"
#include "incidencewrapper.h"
#include "filter.h"

#include "collection.h"
#include "collectioncomboboxmodel.h"
#include "mimetypes.h"

#include <akonadi_version.h>
#include <Akonadi/Collection>
#include <Akonadi/AgentFilterProxyModel>

void MauiCalendarPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QByteArray("org.mauikit.calendar"));

    qmlRegisterType<IncidenceWrapper>(uri, 1, 0, "IncidenceWrapper");
    qmlRegisterType<MultiDayIncidenceModel>(uri, 1, 0, "MultiDayIncidenceModel");
    qmlRegisterType<IncidenceOccurrenceModel>(uri, 1, 0, "IncidenceOccurrenceModel");
    qmlRegisterType<HourlyIncidenceModel>(uri, 1, 0, "HourlyIncidenceModel");
    qmlRegisterType<TimeZoneListModel>(uri, 1, 0, "TimeZoneListModel");
    qmlRegisterType<MonthModel>(uri, 1, 0, "MonthModel");
    qmlRegisterType<InfiniteCalendarViewModel>(uri, 1, 0, "InfiniteCalendarViewModel");

    qmlRegisterSingletonType<CalendarManager>(uri, 1, 0, "CalendarManager", [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
        Q_UNUSED(scriptEngine)
        auto cal = CalendarManager::instance();
        engine->setObjectOwnership(cal, QQmlEngine::CppOwnership);
        return cal;
    });

    qmlRegisterSingletonType<Filter>(uri, 1, 0, "Filter", [](QQmlEngine *engine, QJSEngine *scriptEngine) {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        return new Filter;
    });

    qmlRegisterSingletonType<Akonadi::Quick::MimeTypes>(uri, 1, 0, "MimeTypes", [](QQmlEngine *engine, QJSEngine *scriptEngine) {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        return new Akonadi::Quick::MimeTypes;
    });

    qmlRegisterType<Akonadi::Quick::CollectionComboBoxModel>(uri, 1, 0, "CollectionComboBoxModel");
    qmlRegisterUncreatableType<Akonadi::Quick::Collection>(uri, 1, 0, "Collection", QStringLiteral("It's just an enum"));
    qRegisterMetaType<Akonadi::ETMCalendar::Ptr>();
    qRegisterMetaType<QAbstractProxyModel *>("QAbstractProxyModel*");
    qRegisterMetaType<Akonadi::AgentFilterProxyModel *>();
}

#include "moc_plugin.cpp"
