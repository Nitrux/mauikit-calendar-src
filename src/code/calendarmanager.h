// SPDX-FileCopyrightText: 2021 Carl Schwan <carlschwan@kde.org>
// SPDX-FileCopyrightText: 2021 Claudio Cambra <claudio.cambra@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtQmlIntegration/qqmlintegration.h>

#include <QAbstractItemModel>
#include <QColor>
#include <QDateTime>
#include <QJSEngine>
#include <QLockFile>
#include <QObject>
#include <QQmlEngine>
#include <QStandardItemModel>
#include <QSharedPointer>
#include <QStringList>
#include <QVariant>
#include <QSet>
#include <QHash>
#include <QVariantList>

#include <KCalendarCore/FileStorage>
#include <KCalendarCore/MemoryCalendar>

#include "calendar_export.h"

#include "incidencewrapper.h"

class QTimer;
class QTcpServer;
class QTcpSocket;

/**
 *  Owns the application calendar and persists incidence changes.
 *
 * This singleton exposes the available collections, reports loading state, and
 * provides the add, edit, delete, and save operations used by QML clients.
 */
class CALENDAR_EXPORT CalendarManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_CLASSINFO("D-Bus Interface", "org.maui.agenda")

    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QAbstractItemModel *collections READ collections CONSTANT)
    Q_PROPERTY(QStringList subscriptions READ subscriptions NOTIFY subscriptionsChanged)
    Q_PROPERTY(QVariantList caldavPermissions READ caldavPermissions NOTIFY caldavPermissionsChanged)
    Q_PROPERTY(qint64 caldavPermissionsCollectionId READ caldavPermissionsCollectionId NOTIFY caldavPermissionsChanged)

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
    QStringList subscriptions() const;
    QVariantList caldavPermissions() const;
    qint64 caldavPermissionsCollectionId() const;
    KCalendarCore::MemoryCalendar::Ptr calendar() const;
    QColor collectionColor(qint64 collectionId) const;
    Q_INVOKABLE bool setCollectionColor(qint64 collectionId, const QColor &color);
    Q_INVOKABLE bool isCollectionReadOnly(qint64 collectionId) const;
    Q_INVOKABLE bool setCollectionReadOnly(qint64 collectionId, bool readOnly);
    Q_INVOKABLE QString subscriptionForCollection(qint64 collectionId) const;
    KCalendarCore::Incidence::List childIncidences(const QString &uid) const;

    Q_INVOKABLE void save();
    Q_INVOKABLE bool importFile(const QString &filePath);
    Q_INVOKABLE bool exportFile(const QString &filePath);
    Q_INVOKABLE void importUrl(const QString &url);
    Q_INVOKABLE void subscribeUrl(const QString &url);
    Q_INVOKABLE void unsubscribeUrl(const QString &url);
    Q_INVOKABLE bool addCollection(const QString &name);
    Q_INVOKABLE bool removeCollection(qint64 collectionId);
    Q_INVOKABLE void syncCalDav(const QString &url, const QString &username = {}, const QString &password = {}, qint64 collectionId = -1);
    Q_INVOKABLE bool startCalDavOAuthAuthorization(const QString &calendarUrl, const QString &authorizationUrl, const QString &tokenUrl, const QString &clientId, const QString &clientSecret = {});
    Q_INVOKABLE bool isCalDavCollection(qint64 collectionId) const;
    Q_INVOKABLE void loadCalDavPermissions(qint64 collectionId);
    Q_INVOKABLE bool saveCalDavPermissions(const QVariantList &permissions);
    Q_INVOKABLE void resolveSyncConflict(const QString &uid, bool keepLocal);
    Q_SCRIPTABLE Q_INVOKABLE QStringList upcomingEvents(int days = 7) const;
    Q_SCRIPTABLE Q_INVOKABLE QStringList eventsForDate(const QString &date) const;
    Q_INVOKABLE void addIncidence(IncidenceWrapper *incidenceWrapper);
    Q_INVOKABLE void editIncidence(IncidenceWrapper *incidenceWrapper, const QDateTime &occurrenceStart = {});
    Q_INVOKABLE void deleteIncidence(KCalendarCore::Incidence::Ptr incidence, bool deleteChildren = false);
    Q_INVOKABLE void deleteIncidenceOccurrence(KCalendarCore::Incidence::Ptr incidence, const QDateTime &occurrenceStart);
    Q_INVOKABLE void deleteIncidenceSeries(KCalendarCore::Incidence::Ptr incidence);
    Q_INVOKABLE bool hasChildren(KCalendarCore::Incidence::Ptr incidence);
    Q_INVOKABLE QVariant getIncidenceSubclassed(KCalendarCore::Incidence::Ptr incidencePtr);

Q_SIGNALS:
    void loadingChanged();
    void calendarChanged();
    void subscriptionsChanged();
    void caldavPermissionsChanged();
    void caldavPermissionsError(const QString &message);
    void caldavPermissionsSaved();
    void caldavOAuthError(const QString &message);
    void caldavOAuthAuthorized();
    void syncConflict(const QString &uid);

private Q_SLOTS:
    void handleNotificationAction(uint notificationId, const QString &action);
    void handleOAuthConnection();

private:
    void mergeCalendar(const KCalendarCore::MemoryCalendar::Ptr &source, bool preferRemote, qint64 collectionId = -1);
    bool importCalendarData(const QByteArray &data, bool preferRemote, qint64 collectionId = -1);
    void importUrl(const QString &url, qint64 collectionId);
    qint64 ensureSubscriptionCollection(const QString &url);
    qint64 ensureCalDavCollection(const QString &url);
    void saveSubscriptionMetadata() const;
    void uploadCalDav(const QString &url, const QString &username, const QString &password, qint64 collectionId = -1);
    void saveCollectionNames() const;
    void saveCollectionColors() const;
    void saveCalDavAccounts();
    void syncSavedCalDavAccounts();
    void syncCalDavAccount(const QVariantMap &account);
    void refreshCalDavToken(const QVariantMap &account);
    void startCalDavDiscovery(const QString &requestUrl, const QString &originalUrl, const QString &username, const QString &password, qint64 collectionId);
    void syncCalDavEndpoint(const QString &url, const QString &username, const QString &password, qint64 collectionId);
    void setServerCollectionReadOnly(qint64 collectionId, bool readOnly);
    void saveSnoozedReminders() const;
    void saveRemoteState(const QString &url, const QSet<QString> &uids) const;
    void markRemoteDeletion(const QString &uid, qint64 collectionId = -1);
    void deleteCalDavResources(const QString &url, const QStringList &uids, const QString &username, const QString &password);
    void saveDeletedRemoteState(const QString &url, const QSet<QString> &uids) const;
    QSet<QString> deletedRemoteState(const QString &url) const;
    QSet<QString> remoteState(const QString &url) const;
    QSet<QString> calendarUids(const QByteArray &data) const;
    KCalendarCore::Incidence::List calendarIncidences(const QByteArray &data) const;

    KCalendarCore::MemoryCalendar::Ptr m_calendar;
    KCalendarCore::FileStorage *m_storage = nullptr;
    QStandardItemModel *m_collections = nullptr;
    QTimer *m_reminderTimer = nullptr;
    QTimer *m_subscriptionTimer = nullptr;
    QStringList m_subscriptions;
    QSet<QString> m_notifiedReminders;
    QHash<uint, QString> m_notificationKeys;
    QHash<QString, QDateTime> m_snoozedReminders;
    bool m_loading = true;
    QHash<QString, QSet<QString>> m_remoteUids;
    QSharedPointer<QLockFile> m_reminderLock;
    QHash<QString, QByteArray> m_remoteEtags;
    QHash<QString, qint64> m_subscriptionCollectionIds;
    QSet<qint64> m_readOnlyCollections;
    QSet<qint64> m_serverReadOnlyCollections;
    QSet<QString> m_caldavDiscoveryUrls;
    QHash<qint64, QColor> m_collectionColors;
    QStringList m_collectionNames;
    QStringList m_caldavUrls;
    QVariantList m_caldavAccounts;
    QVariantList m_caldavPermissions;
    qint64 m_caldavPermissionsCollectionId = -1;
    QTimer *m_caldavTimer = nullptr;
    bool m_syncingAccounts = false;
    QTcpServer *m_oauthServer = nullptr;
    QString m_oauthCalendarUrl;
    QString m_oauthAuthorizationUrl;
    QString m_oauthTokenUrl;
    QString m_oauthClientId;
    QString m_oauthClientSecret;
    QString m_oauthState;
    QString m_oauthVerifier;
    QHash<QString, KCalendarCore::Incidence::List> m_remoteConflicts;
    QHash<QString, QString> m_conflictUrls;
};

