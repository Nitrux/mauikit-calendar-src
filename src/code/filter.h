// SPDX-FileCopyrightText: 2022 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQmlIntegration/qqmlintegration.h>
#include "calendar_export.h"
/**
 *  Stores shared collection, tag, and text criteria for calendar models.
 *
 * The singleton can toggle individual tags or reset all criteria at once.
 */
class CALENDAR_EXPORT Filter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(qint64 collectionId READ collectionId WRITE setCollectionId NOTIFY collectionIdChanged)
    Q_PROPERTY(QStringList collectionIds READ collectionIds WRITE setCollectionIds NOTIFY collectionIdsChanged)
    Q_PROPERTY(QStringList tags READ tags WRITE setTags NOTIFY tagsChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

public:
    qint64 collectionId() const;
    QStringList tags() const;
    QStringList collectionIds() const;
    QString name() const;

public Q_SLOTS:
    void setCollectionId(const qint64 collectionId);
    void setTags(const QStringList tags);
    void setCollectionIds(const QStringList collectionIds);
    void toggleCollection(const qint64 collectionId);
    void setName(const QString &name);

    void toggleFilterTag(const QString tagName);
    void reset();
    void removeTag(const QString &tagName);

Q_SIGNALS:
    void collectionIdChanged();
    void tagsChanged();
    void collectionIdsChanged();
    void nameChanged();

private:
    qint64 m_collectionId = -1;
    QStringList m_tags;
    QStringList m_collectionIds;
    QString m_name;
};
