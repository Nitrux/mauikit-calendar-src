// SPDX-FileCopyrightText: 2007-2009 Tobias Koenig <tokoe@kde.org>
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include "calendar_export.h"

/**
 * @short A model listing the available local calendars for use in a ComboBox.
 *
 * Provides a simple list of local calendars stored as .ics files.
 * The mimeTypeFilter and accessRightsFilter properties are accepted but ignored
 * (all local calendars accept all types and are writable).
 */
class CALENDAR_EXPORT CollectionComboBoxModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList mimeTypeFilter READ mimeTypeFilter WRITE setMimeTypeFilter NOTIFY mimeTypeFilterChanged)
    Q_PROPERTY(int accessRightsFilter READ accessRightsFilter WRITE setAccessRightsFilter NOTIFY accessRightsFilterChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(qint64 defaultCollectionId READ defaultCollectionId WRITE setDefaultCollectionId NOTIFY defaultCollectionIdChanged)

public:
    enum Roles {
        CollectionIdRole = Qt::UserRole + 1,
        CollectionColorRole = Qt::BackgroundRole,
    };

    explicit CollectionComboBoxModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QStringList mimeTypeFilter() const;
    void setMimeTypeFilter(const QStringList &mimetypes);

    int accessRightsFilter() const;
    void setAccessRightsFilter(int rights);

    qint64 defaultCollectionId() const;
    void setDefaultCollectionId(qint64 collectionId);

    int currentIndex() const;
    void setCurrentIndex(int index);

Q_SIGNALS:
    void mimeTypeFilterChanged();
    void accessRightsFilterChanged();
    void defaultCollectionIdChanged();
    void currentIndexChanged();

private:
    struct LocalCalendar {
        QString name;
        QColor color;
    };

    QList<LocalCalendar> m_calendars;
    int m_currentIndex = 0;
    qint64 m_defaultCollectionId = 0;
};
