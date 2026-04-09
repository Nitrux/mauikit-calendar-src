// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "mimetypes.h"

MimeTypes::MimeTypes(QObject *parent)
    : QObject(parent)
{
}

QString MimeTypes::calendar() const
{
    return QStringLiteral("text/calendar");
}

QString MimeTypes::todo() const
{
    return QStringLiteral("text/calendar");
}
