#include "plugin.h"

#include "calendarmanager.h"
#include "collection.h"
#include "collectioncomboboxmodel.h"
#include "filter.h"
#include "incidencewrapper.h"
#include "mimetypes.h"
#include "models/hourlyincidencemodel.h"
#include "models/incidenceoccurrencemodel.h"
#include "models/infinitecalendarviewmodel.h"
#include "models/monthmodel.h"
#include "models/multidayincidencemodel.h"
#include "models/timezonelistmodel.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <qqml.h>

void MauiCalendarPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.mauikit.calendar"));

    qmlRegisterModule(uri, 1, 0);

    qmlRegisterSingletonType<CalendarManager>(uri, 1, 0, "CalendarManager", &CalendarManager::create);
    qmlRegisterSingletonType<Filter>(uri, 1, 0, "Filter", [](QQmlEngine *, QJSEngine *) -> QObject * {
        return new Filter;
    });
    qmlRegisterSingletonType<MimeTypes>(uri, 1, 0, "MimeTypes", &MimeTypes::create);

    qmlRegisterUncreatableType<Collection>(uri, 1, 0, "Collection", QStringLiteral(""));
    qmlRegisterType<CollectionComboBoxModel>(uri, 1, 0, "CollectionComboBoxModel");
    qmlRegisterType<IncidenceWrapper>(uri, 1, 0, "IncidenceWrapper");
    qmlRegisterType<HourlyIncidenceModel>(uri, 1, 0, "HourlyIncidenceModel");
    qmlRegisterType<IncidenceOccurrenceModel>(uri, 1, 0, "IncidenceOccurrenceModel");
    qmlRegisterType<InfiniteCalendarViewModel>(uri, 1, 0, "InfiniteCalendarViewModel");
    qmlRegisterType<MonthModel>(uri, 1, 0, "MonthModel");
    qmlRegisterType<MultiDayIncidenceModel>(uri, 1, 0, "MultiDayIncidenceModel");
    qmlRegisterType<TimeZoneListModel>(uri, 1, 0, "TimeZoneListModel");
}

#include "moc_plugin.cpp"
