#include "plugin.h"

#include <QQmlEngine>

void MauiCalendarPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QByteArray("org.mauikit.calendar"));
}

#include "moc_plugin.cpp"
