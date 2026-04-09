// SPDX-FileCopyrightText: 2007-2009 Tobias Koenig <tokoe@kde.org>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "collectioncomboboxmodel.h"

#include <KLocalizedString>

CollectionComboBoxModel::CollectionComboBoxModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_calendars.append({i18n("Personal"), QColor(0x1d, 0x99, 0xf3)}); // default blue
}

int CollectionComboBoxModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_calendars.size();
}

QVariant CollectionComboBoxModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_calendars.size()) {
        return {};
    }
    const auto &cal = m_calendars.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return cal.name;
    case Qt::BackgroundRole: // CollectionColorRole
        return cal.color;
    case CollectionIdRole: // also Qt::UserRole + 1 = Collection::CollectionRole
        return static_cast<qint64>(index.row());
    default:
        return {};
    }
}

QHash<int, QByteArray> CollectionComboBoxModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {Qt::BackgroundRole, "collectionColor"},
        {CollectionIdRole, "collectionId"},
    };
}

QStringList CollectionComboBoxModel::mimeTypeFilter() const
{
    return {};
}

void CollectionComboBoxModel::setMimeTypeFilter(const QStringList &)
{
    // no-op: local calendars accept all types
    Q_EMIT mimeTypeFilterChanged();
}

int CollectionComboBoxModel::accessRightsFilter() const
{
    return 0;
}

void CollectionComboBoxModel::setAccessRightsFilter(int)
{
    // no-op: local calendars are always writable
    Q_EMIT accessRightsFilterChanged();
}

qint64 CollectionComboBoxModel::defaultCollectionId() const
{
    return m_defaultCollectionId;
}

void CollectionComboBoxModel::setDefaultCollectionId(qint64 collectionId)
{
    if (m_defaultCollectionId == collectionId) {
        return;
    }
    m_defaultCollectionId = collectionId;
    // Select the row matching this id (which is just the row index)
    const int row = static_cast<int>(collectionId);
    if (row >= 0 && row < m_calendars.size()) {
        setCurrentIndex(row);
    }
    Q_EMIT defaultCollectionIdChanged();
}

int CollectionComboBoxModel::currentIndex() const
{
    return m_currentIndex;
}

void CollectionComboBoxModel::setCurrentIndex(int index)
{
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    Q_EMIT currentIndexChanged();
}

#include "moc_collectioncomboboxmodel.cpp"
