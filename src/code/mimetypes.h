// SPDX-FileCopyrightText: 2007-2009 Tobias Koenig <tokoe@kde.org>
// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include "calendar_export.h"

class CALENDAR_EXPORT MimeTypes : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString calendar READ calendar CONSTANT)
    Q_PROPERTY(QString todo READ todo CONSTANT)

public:
    explicit MimeTypes(QObject *parent = nullptr);
    static MimeTypes *create(QQmlEngine *, QJSEngine *) { return new MimeTypes; }
    QString calendar() const;
    QString todo() const;
};
