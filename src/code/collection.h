// SPDX-FileCopyrightText: 2007-2009 Tobias Koenig <tokoe@kde.org>
// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include "calendar_export.h"

class CALENDAR_EXPORT Collection : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
public:
    enum Right {
        ReadOnly = 0x0,
        CanChangeItem = 0x1,
        CanCreateItem = 0x2,
        CanDeleteItem = 0x4,
        CanChangeCollection = 0x8,
        CanCreateCollection = 0x10,
        CanDeleteCollection = 0x20,
        AllRights = (CanChangeItem | CanCreateItem | CanDeleteItem | CanChangeCollection | CanCreateCollection | CanDeleteCollection),
    };
    Q_ENUM(Right)

    enum Role {
        CollectionRole = Qt::UserRole + 1,
        CollectionColorRole = Qt::BackgroundRole,
    };
    Q_ENUM(Role)
};
