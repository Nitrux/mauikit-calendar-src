// SPDX-FileCopyrightText: 2021 Carl Schwan <carlschwan@kde.org>
// SPDX-FileCopyrightText: 2021 Claudio Cambra <claudio.cambra@gmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "calendarmanager.h"
#include "incidencewrapper.h"
#include "collection.h"

#include <KCalendarCore/Event>
#include <KCalendarCore/FileStorage>
#include <KCalendarCore/Journal>
#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/OccurrenceIterator>
#include <KCalendarCore/Todo>
#include <KLocalizedString>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QAuthenticator>
#include <QTemporaryFile>
#include <QUrl>
#include <QColor>
#include <QTimer>
#include <QSet>
#include <QSettings>
#include <QTimeZone>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDesktopServices>

#include <utility>

namespace {

void applyCalDavAuthorization(QNetworkRequest &request, const QString &username, const QString &secret)
{
    if (secret.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)) {
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + secret.mid(7).trimmed().toUtf8());
    } else if (!username.isEmpty()) {
        const auto credentials = (username + QStringLiteral(":" ) + secret).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + credentials);
    }
}

void configureCalDavAuthentication(QNetworkAccessManager *network, const QString &username, const QString &secret)
{
    if (!network || username.isEmpty() || secret.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)) return;
    QObject::connect(network, &QNetworkAccessManager::authenticationRequired, network, [username, secret](QNetworkReply *, QAuthenticator *authenticator) {
        authenticator->setUser(username);
        authenticator->setPassword(secret);
    });
}

QString calDavCredential(const QVariantMap &account)
{
    const auto secret = account.value(QStringLiteral("password")).toString();
    return account.value(QStringLiteral("authType"), QStringLiteral("basic")).toString() == QStringLiteral("bearer") ? QStringLiteral("Bearer ") + secret : secret;
}

QVariantList parseCalDavPermissions(const QByteArray &data)
{
    QVariantList permissions;
    QXmlStreamReader xml(data);
    bool inAce = false;
    bool inPrincipal = false;
    bool inGrant = false;
    bool inDeny = false;
    bool inHref = false;
    QString principal;
    QString principalType;
    QString currentPrivilege;
    QStringList granted;
    QStringList denied;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const auto name = xml.name().toString();
            if (name == QStringLiteral("ace")) {
                inAce = true;
                principal.clear();
                principalType.clear();
                granted.clear();
                denied.clear();
            } else if (inAce && name == QStringLiteral("principal")) {
                inPrincipal = true;
            } else if (inPrincipal && name == QStringLiteral("href")) {
                inHref = true;
            } else if (inPrincipal && (name == QStringLiteral("all") || name == QStringLiteral("authenticated") || name == QStringLiteral("unauthenticated"))) {
                principalType = name;
            } else if (inAce && name == QStringLiteral("grant")) {
                inGrant = true;
            } else if (inAce && name == QStringLiteral("deny")) {
                inDeny = true;
            } else if (inAce && name == QStringLiteral("privilege")) {
                currentPrivilege.clear();
            } else if (inAce && (name == QStringLiteral("read") || name == QStringLiteral("write") || name == QStringLiteral("write-content") || name == QStringLiteral("write-properties") || name == QStringLiteral("bind") || name == QStringLiteral("unbind")) && (inGrant || inDeny)) {
                currentPrivilege = name;
            }
        } else if (xml.isCharacters() && inHref) {
            principal += xml.text().toString().trimmed();
        } else if (xml.isEndElement()) {
            const auto name = xml.name().toString();
            if (name == QStringLiteral("href")) {
                inHref = false;
            } else if (name == QStringLiteral("principal")) {
                inPrincipal = false;
            } else if (name == QStringLiteral("grant")) {
                inGrant = false;
            } else if (name == QStringLiteral("deny")) {
                inDeny = false;
            } else if (name == QStringLiteral("privilege") && !currentPrivilege.isEmpty()) {
                if (inGrant) granted.append(currentPrivilege);
                if (inDeny) denied.append(currentPrivilege);
                currentPrivilege.clear();
            } else if (name == QStringLiteral("ace")) {
                const auto principalValue = principalType.isEmpty() ? principal : principalType;
                if (!principalValue.isEmpty()) {
                    permissions.append(QVariantMap{{QStringLiteral("principal"), principalValue}, {QStringLiteral("read"), granted.contains(QStringLiteral("read"))}, {QStringLiteral("write"), granted.contains(QStringLiteral("write")) || granted.contains(QStringLiteral("write-content")) || granted.contains(QStringLiteral("write-properties"))}, {QStringLiteral("denied"), denied}});
                }
                inAce = false;
            }
        }
    }
    return permissions;
}

QByteArray serializeCalDavPermissions(const QVariantList &permissions)
{
    QByteArray data;
    QXmlStreamWriter xml(&data);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("d:acl"));
    xml.writeNamespace(QStringLiteral("DAV:"), QStringLiteral("d"));
    for (const auto &permissionValue : permissions) {
        const auto permission = permissionValue.toMap();
        const auto principal = permission.value(QStringLiteral("principal")).toString().trimmed();
        if (principal.isEmpty()) continue;
        xml.writeStartElement(QStringLiteral("d:ace"));
        xml.writeStartElement(QStringLiteral("d:principal"));
        if (principal == QStringLiteral("all") || principal == QStringLiteral("authenticated") || principal == QStringLiteral("unauthenticated"))
            xml.writeEmptyElement(QStringLiteral("d:") + principal);
        else
            xml.writeTextElement(QStringLiteral("d:href"), principal);
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("d:grant"));
        if (permission.value(QStringLiteral("read")).toBool()) {
            xml.writeStartElement(QStringLiteral("d:privilege"));
            xml.writeEmptyElement(QStringLiteral("d:read"));
            xml.writeEndElement();
        }
        if (permission.value(QStringLiteral("write")).toBool()) {
            for (const auto &privilege : {QStringLiteral("write-content"), QStringLiteral("write-properties")}) {
                xml.writeStartElement(QStringLiteral("d:privilege"));
                xml.writeEmptyElement(QStringLiteral("d:") + privilege);
                xml.writeEndElement();
            }
        }
        xml.writeEndElement();
        const auto denied = permission.value(QStringLiteral("denied")).toStringList();
        if (!denied.isEmpty()) {
            xml.writeStartElement(QStringLiteral("d:deny"));
            for (const auto &privilege : denied) {
                xml.writeStartElement(QStringLiteral("d:privilege"));
                xml.writeEmptyElement(QStringLiteral("d:") + privilege);
                xml.writeEndElement();
            }
            xml.writeEndElement();
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    return data;
}

struct CalDavDiscoveryResult
{
    QStringList calendars;
    QUrl calendarHome;
    QUrl principal;
};

CalDavDiscoveryResult parseCalDavDiscovery(const QByteArray &data, const QUrl &baseUrl)
{
    CalDavDiscoveryResult result;
    QXmlStreamReader xml(data);
    bool inResponse = false;
    bool inResourceType = false;
    bool responseIsCalendar = false;
    bool responseHrefSeen = false;
    bool inCalendarHome = false;
    bool inPrincipal = false;
    bool inHref = false;
    QString href;
    QUrl responseHref;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("response")) {
                inResponse = true;
                responseHref = {};
                responseHrefSeen = false;
                responseIsCalendar = false;
            } else if (xml.name() == QLatin1String("resourcetype")) {
                inResourceType = true;
            } else if (inResourceType && xml.name() == QLatin1String("calendar")) {
                responseIsCalendar = true;
            } else if (xml.name() == QLatin1String("calendar-home-set")) {
                inCalendarHome = true;
            } else if (xml.name() == QLatin1String("current-user-principal")) {
                inPrincipal = true;
            } else if (xml.name() == QLatin1String("href")) {
                inHref = true;
                href.clear();
            }
        } else if (xml.isCharacters() && inHref) {
            href.append(xml.text().toString());
        } else if (xml.isEndElement()) {
            if (xml.name() == QLatin1String("href")) {
                const auto resolved = baseUrl.resolved(QUrl(href.trimmed()));
                if (inCalendarHome) result.calendarHome = resolved;
                if (inPrincipal) result.principal = resolved;
                if (inResponse && !responseHrefSeen) {
                    responseHref = resolved;
                    responseHrefSeen = true;
                }
                inHref = false;
            } else if (xml.name() == QLatin1String("calendar-home-set")) {
                inCalendarHome = false;
            } else if (xml.name() == QLatin1String("current-user-principal")) {
                inPrincipal = false;
            } else if (xml.name() == QLatin1String("resourcetype")) {
                inResourceType = false;
            } else if (xml.name() == QLatin1String("response")) {
                if (responseIsCalendar && responseHref.isValid()) {
                    const auto responseUrl = responseHref.toString(QUrl::FullyEncoded);
                    if (!result.calendars.contains(responseUrl)) result.calendars.append(responseUrl);
                }
                inResponse = false;
            }
        }
    }
    return result;
}

QByteArray incidenceStoredTimeZone(const KCalendarCore::Incidence::Ptr &incidence)
{
    const auto stored = incidence->customProperty("MauiCalendar", "TimeZone");
    if (!stored.isEmpty()) return stored.toUtf8();
    const auto id = incidence->dtStart().timeZone().id();
    return id.isEmpty() ? QTimeZone::systemTimeZoneId() : id;
}

QDateTime rawIncidenceEnd(const KCalendarCore::Incidence::Ptr &incidence)
{
    if (incidence->type() == KCalendarCore::IncidenceBase::TypeEvent)
        return incidence.staticCast<KCalendarCore::Event>()->dtEnd();
    if (incidence->type() == KCalendarCore::IncidenceBase::TypeTodo)
        return incidence.staticCast<KCalendarCore::Todo>()->dtDue();
    return {};
}

void setRawIncidenceEnd(const KCalendarCore::Incidence::Ptr &incidence, const QDateTime &value)
{
    if (incidence->type() == KCalendarCore::IncidenceBase::TypeEvent)
        incidence.staticCast<KCalendarCore::Event>()->setDtEnd(value);
    else if (incidence->type() == KCalendarCore::IncidenceBase::TypeTodo)
        incidence.staticCast<KCalendarCore::Todo>()->setDtDue(value);
}

bool normalizeIncidenceTimes(const KCalendarCore::Incidence::Ptr &incidence)
{
    if (!incidence || incidence->allDay()) return false;
    const auto zoneId = incidenceStoredTimeZone(incidence);
    bool changed = incidence->customProperty("MauiCalendar", "TimeZone") != QString::fromUtf8(zoneId);
    incidence->setCustomProperty("MauiCalendar", "TimeZone", QString::fromUtf8(zoneId));
    if (zoneId == QByteArrayLiteral("Floating")) return changed;
    const auto start = incidence->dtStart();
    const auto normalizedStart = start.toUTC();
    if (start.timeZone().id() != QByteArrayLiteral("UTC")) {
        incidence->setDtStart(normalizedStart);
        changed = true;
    }
    const auto end = rawIncidenceEnd(incidence);
    const auto normalizedEnd = end.toUTC();
    if (end.isValid() && end.timeZone().id() != QByteArrayLiteral("UTC")) {
        setRawIncidenceEnd(incidence, normalizedEnd);
        changed = true;
    }
    if (incidence->recurs()) {
        const auto recurrence = incidence->recurrence();
        const auto recurrenceStart = recurrence->startDateTime();
        if (recurrenceStart.isValid() && recurrenceStart.timeZone().id() != QByteArrayLiteral("UTC")) {
            recurrence->setStartDateTime(recurrenceStart.toUTC(), false);
            changed = true;
        }
        const auto recurrenceEnd = recurrence->endDateTime();
        if (recurrenceEnd.isValid() && recurrenceEnd.timeZone().id() != QByteArrayLiteral("UTC")) {
            recurrence->setEndDateTime(recurrenceEnd.toUTC());
            changed = true;
        }
    }
    return changed;
}

KCalendarCore::Incidence::Ptr incidenceForSerialization(const KCalendarCore::Incidence::Ptr &source)
{
    auto incidence = KCalendarCore::Incidence::Ptr(source->clone());
    if (!incidence || incidence->allDay()) return incidence;
    const auto zoneId = incidenceStoredTimeZone(incidence);
    if (zoneId == QByteArrayLiteral("Floating") || zoneId == QByteArrayLiteral("UTC")) return incidence;
    const QTimeZone zone(zoneId);
    if (!zone.isValid()) return incidence;
    incidence->setDtStart(incidence->dtStart().toTimeZone(zone));
    const auto end = rawIncidenceEnd(incidence);
    if (end.isValid()) setRawIncidenceEnd(incidence, end.toTimeZone(zone));
    if (incidence->recurs()) {
        const auto recurrence = incidence->recurrence();
        if (recurrence->startDateTime().isValid()) recurrence->setStartDateTime(recurrence->startDateTime().toTimeZone(zone), false);
        if (recurrence->endDateTime().isValid()) recurrence->setEndDateTime(recurrence->endDateTime().toTimeZone(zone));
    }
    return incidence;
}

}

Q_GLOBAL_STATIC(CalendarManager, calendarManagerGlobalInstance)

CalendarManager *CalendarManager::instance()
{
    return calendarManagerGlobalInstance;
}

CalendarManager::CalendarManager(QObject *parent)
    : QObject(parent)
{
    m_calendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString filePath = dataDir + QStringLiteral("/default.ics");

    m_storage = new KCalendarCore::FileStorage(m_calendar, filePath);

    m_reminderLock = QSharedPointer<QLockFile>::create(dataDir + QStringLiteral("/reminders.lock"));
    m_reminderLock->setStaleLockTime(0);
    if (!m_storage->open()) {
        qInfo() << "Calendar file not found, will create on first save:" << filePath;
    } else if (!m_storage->load()) {
        qWarning() << "Failed to load calendar from" << filePath;
    }

    bool normalizedTimes = false;
    for (const auto &incidence : m_calendar->incidences()) {
        if (incidence->customProperty("MauiCalendar", "CollectionId").isEmpty())
            incidence->setCustomProperty("MauiCalendar", "CollectionId", QStringLiteral("0"));
        normalizedTimes = normalizeIncidenceTimes(incidence) || normalizedTimes;
    }
    if (normalizedTimes) m_storage->save();

    QSettings settings;
    m_collectionNames = settings.value(QStringLiteral("Agenda/calendarNames"), QStringList {i18n("Personal"), i18n("Work"), i18n("Shared")}).toStringList();
    if (m_collectionNames.isEmpty()) m_collectionNames.append(i18n("Personal"));
    for (const auto &id : settings.value(QStringLiteral("Agenda/readOnlyCollections")).toStringList()) m_readOnlyCollections.insert(id.toLongLong());
    for (const auto &entry : settings.value(QStringLiteral("Agenda/subscriptionCollectionIds")).toStringList()) {
        const auto separator = entry.lastIndexOf(QLatin1Char(9));
        if (separator > 0) m_subscriptionCollectionIds.insert(entry.left(separator), entry.mid(separator + 1).toLongLong());
    }
    m_subscriptions = settings.value(QStringLiteral("Agenda/calendarSubscriptions")).toStringList();
    for (const auto &entry : settings.value(QStringLiteral("Agenda/snoozedReminders")).toStringList()) {
        const auto separator = entry.lastIndexOf(QLatin1Char(9));
        if (separator > 0) {
            const auto time = QDateTime::fromString(entry.mid(separator + 1), Qt::ISODate);
            if (time.isValid()) m_snoozedReminders.insert(entry.left(separator), time);
        }
    }
    m_caldavUrls = settings.value(QStringLiteral("Agenda/caldavUrls")).toStringList();
    for (const auto &entry : settings.value(QStringLiteral("Agenda/calendarColors")).toStringList()) {
        const auto separator = entry.indexOf(QLatin1Char(9));
        if (separator > 0) {
            const auto color = QColor(entry.mid(separator + 1));
            if (color.isValid()) m_collectionColors.insert(entry.left(separator).toLongLong(), color);
        }
    }
    m_caldavAccounts = settings.value(QStringLiteral("Agenda/caldavAccounts")).toList();
    if (m_caldavAccounts.isEmpty()) {
        for (const auto &url : std::as_const(m_caldavUrls))
            m_caldavAccounts.append(QVariantMap{{QStringLiteral("url"), url}, {QStringLiteral("username"), QString()}, {QStringLiteral("password"), QString()}, {QStringLiteral("enabled"), true}});
    } else {
        for (const auto &account : std::as_const(m_caldavAccounts)) {
            const auto url = account.toMap().value(QStringLiteral("url")).toString();
            if (!url.isEmpty() && !m_caldavUrls.contains(url)) m_caldavUrls.append(url);
        }
    }

    m_collections = new QStandardItemModel(this);
    m_collections->setItemRoleNames({{Qt::DisplayRole, "display"}, {Qt::CheckStateRole, "checkState"}, {Qt::UserRole + 1, "collectionId"}, {Qt::UserRole + 2, "collectionColor"}, {Qt::UserRole + 3, "accessRights"}});
    if (m_collectionNames.isEmpty()) m_collectionNames.append(i18n("Personal"));
    for (int i = 0; i < m_collectionNames.size(); ++i) {
        auto item = new QStandardItem(m_collectionNames.at(i));
        item->setCheckable(true);
        item->setCheckState(Qt::Checked);
        item->setData(i, Qt::UserRole + 1);
        item->setData(collectionColor(i), Qt::UserRole + 2);
        item->setData(m_readOnlyCollections.contains(i) ? static_cast<int>(Collection::ReadOnly) : static_cast<int>(Collection::AllRights), Qt::UserRole + 3);
        m_collections->appendRow(item);
    }
    for (const auto &subscription : m_subscriptions) ensureSubscriptionCollection(subscription);
    bool caldavAccountsChanged = false;
    for (int i = 0; i < m_caldavAccounts.size(); ++i) {
        auto account = m_caldavAccounts.at(i).toMap();
        auto collectionId = account.value(QStringLiteral("collectionId"), -1).toLongLong();
        if (collectionId < 0 || collectionId >= m_collectionNames.size()) {
            collectionId = ensureCalDavCollection(account.value(QStringLiteral("url")).toString());
            account.insert(QStringLiteral("collectionId"), collectionId);
            m_caldavAccounts[i] = account;
            caldavAccountsChanged = true;
        }
    }
    if (caldavAccountsChanged) saveCalDavAccounts();
    m_reminderTimer = new QTimer(this);
    m_reminderTimer->setInterval(30000);
    connect(m_reminderTimer, &QTimer::timeout, this, [this] {
        if (!m_reminderLock->isLocked() && !m_reminderLock->tryLock(0)) return;
        const auto now = QDateTime::currentDateTime();
        QDBusInterface notifications(QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"), QStringLiteral("org.freedesktop.Notifications"), QDBusConnection::sessionBus());
        if (!notifications.isValid()) return;
        KCalendarCore::OccurrenceIterator iterator(*m_calendar, now.addDays(-1), now.addDays(1));
        while (iterator.hasNext()) {
            iterator.next();
            const auto incidence = iterator.incidence();
            if (!incidence) continue;
            const auto occurrenceStart = iterator.occurrenceStartDate();
            const auto alarms = incidence->alarms();
            for (int alarmIndex = 0; alarmIndex < alarms.size(); ++alarmIndex) {
                const auto &alarm = alarms.at(alarmIndex);
                if (!alarm->enabled()) continue;
                const auto trigger = alarm->time().isValid() ? alarm->time() : occurrenceStart.addSecs(alarm->startOffset().asSeconds());
                const auto key = incidence->uid() + QLatin1Char(39) + QString::number(occurrenceStart.toSecsSinceEpoch()) + QLatin1Char(39) + QString::number(alarmIndex) + QLatin1Char(39) + QString::number(trigger.toSecsSinceEpoch());
                const auto snoozeAt = m_snoozedReminders.value(key);
                if (snoozeAt.isValid()) {
                    if (snoozeAt > now) continue;
                    m_snoozedReminders.remove(key);
                    saveSnoozedReminders();
                } else {
                    if (trigger > now || trigger.secsTo(now) >= 60 || m_notifiedReminders.contains(key)) continue;
                }
                const auto response = notifications.call(QStringLiteral("Notify"), QStringLiteral("Agenda"), 0, QStringLiteral("agenda"), incidence->summary(), incidence->description(), QStringList {QStringLiteral("snooze"), i18n("Snooze 5 minutes")}, QVariantMap(), 0);
                if (response.type() != QDBusMessage::ErrorMessage && !response.arguments().isEmpty()) {
                    const auto notificationId = response.arguments().first().toUInt();
                    m_notificationKeys.insert(notificationId, key);
                    m_notifiedReminders.insert(key);
                }
            }
        }
    });
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"), QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("ActionInvoked"), this, SLOT(handleNotificationAction(uint,QString)));
    m_reminderTimer->start();

    m_subscriptionTimer = new QTimer(this);
    m_subscriptionTimer->setInterval(15 * 60 * 1000);
    connect(m_subscriptionTimer, &QTimer::timeout, this, [this] {
        for (const auto &subscription : m_subscriptions) importUrl(subscription, m_subscriptionCollectionIds.value(subscription, -1));
    });
    m_subscriptionTimer->start();

    m_caldavTimer = new QTimer(this);
    m_caldavTimer->setInterval(15 * 60 * 1000);
    connect(m_caldavTimer, &QTimer::timeout, this, &CalendarManager::syncSavedCalDavAccounts);
    m_caldavTimer->start();
    QTimer::singleShot(0, this, &CalendarManager::syncSavedCalDavAccounts);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.maui.agenda"));
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Agenda"), this, QDBusConnection::ExportScriptableSlots);
    m_loading = false;
    Q_EMIT loadingChanged();

    // CalendarManager emits calendarChanged() explicitly after every mutation.
}

CalendarManager::~CalendarManager()
{
    save();
}

void CalendarManager::handleNotificationAction(uint notificationId, const QString &action)
{
    if (action != QStringLiteral("snooze")) return;
    const auto key = m_notificationKeys.take(notificationId);
    if (key.isEmpty()) return;
    m_notifiedReminders.remove(key);
    m_snoozedReminders.insert(key, QDateTime::currentDateTime().addSecs(5 * 60));
    saveSnoozedReminders();
}

void CalendarManager::saveSnoozedReminders() const
{
    QStringList values;
    for (auto it = m_snoozedReminders.cbegin(); it != m_snoozedReminders.cend(); ++it)
        values.append(it.key() + QLatin1Char(9) + it.value().toString(Qt::ISODate));
    QSettings().setValue(QStringLiteral("Agenda/snoozedReminders"), values);
}

bool CalendarManager::loading() const
{
    return m_loading;
}

QAbstractItemModel *CalendarManager::collections()
{
    return m_collections;
}

QStringList CalendarManager::subscriptions() const
{
    return m_subscriptions;
}

QVariantList CalendarManager::caldavAccounts() const
{
    QVariantList accounts;
    for (const auto &account : m_caldavAccounts) {
        auto publicAccount = account.toMap();
        const auto authentication = publicAccount.value(QStringLiteral("authType"), QStringLiteral("basic")).toString();
        const auto hasSecret = !publicAccount.value(QStringLiteral("password")).toString().isEmpty();
        publicAccount.insert(QStringLiteral("authType"), authentication);
        publicAccount.insert(QStringLiteral("hasPassword"), authentication != QStringLiteral("bearer") && hasSecret);
        publicAccount.insert(QStringLiteral("hasToken"), authentication == QStringLiteral("bearer") && hasSecret);
        publicAccount.insert(QStringLiteral("hasRefreshToken"), !publicAccount.value(QStringLiteral("refreshToken")).toString().isEmpty());
        publicAccount.remove(QStringLiteral("password"));
        publicAccount.remove(QStringLiteral("refreshToken"));
        publicAccount.remove(QStringLiteral("clientSecret"));
        accounts.append(publicAccount);
    }
    return accounts;
}

QVariantList CalendarManager::caldavPermissions() const
{
    return m_caldavPermissions;
}

qint64 CalendarManager::caldavPermissionsCollectionId() const
{
    return m_caldavPermissionsCollectionId;
}

bool CalendarManager::isCalDavCollection(qint64 collectionId) const
{
    for (const auto &accountValue : m_caldavAccounts) {
        if (accountValue.toMap().value(QStringLiteral("collectionId"), -1).toLongLong() == collectionId) return true;
    }
    return false;
}

void CalendarManager::loadCalDavPermissions(qint64 collectionId)
{
    m_caldavPermissions.clear();
    m_caldavPermissionsCollectionId = -1;
    Q_EMIT caldavPermissionsChanged();
    QVariantMap account;
    for (const auto &accountValue : std::as_const(m_caldavAccounts)) {
        const auto candidate = accountValue.toMap();
        if (candidate.value(QStringLiteral("collectionId"), -1).toLongLong() == collectionId) {
            account = candidate;
            break;
        }
    }
    const QUrl url(account.value(QStringLiteral("url")).toString());
    if (!url.isValid()) {
        Q_EMIT caldavPermissionsError(i18n("The calendar does not have a CalDAV account."));
        return;
    }
    QNetworkRequest request(url);
    request.setRawHeader("Depth", "0");
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml; charset=utf-8"));
    applyCalDavAuthorization(request, account.value(QStringLiteral("username")).toString(), calDavCredential(account));
    const auto body = QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\" ?><d:propfind xmlns:d=\"DAV:\"><d:prop><d:acl/><d:current-user-privilege-set/></d:prop></d:propfind>");
    auto *network = new QNetworkAccessManager(this);
    configureCalDavAuthentication(network, account.value(QStringLiteral("username")).toString(), calDavCredential(account));
    auto *reply = network->sendCustomRequest(request, QByteArrayLiteral("PROPFIND"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, network, collectionId] {
        const auto data = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto permissions = parseCalDavPermissions(data);
        if (reply->error() != QNetworkReply::NoError || status >= 400 || permissions.isEmpty()) {
            Q_EMIT caldavPermissionsError(reply->error() == QNetworkReply::NoError ? i18n("The CalDAV server did not provide editable permissions.") : reply->errorString());
        } else {
            m_caldavPermissions = permissions;
            m_caldavPermissionsCollectionId = collectionId;
            Q_EMIT caldavPermissionsChanged();
        }
        reply->deleteLater();
        network->deleteLater();
    });
}

bool CalendarManager::saveCalDavPermissions(const QVariantList &permissions)
{
    if (m_caldavPermissionsCollectionId < 0 || !isCalDavCollection(m_caldavPermissionsCollectionId)) return false;
    QVariantMap account;
    for (const auto &accountValue : std::as_const(m_caldavAccounts)) {
        const auto candidate = accountValue.toMap();
        if (candidate.value(QStringLiteral("collectionId"), -1).toLongLong() == m_caldavPermissionsCollectionId) {
            account = candidate;
            break;
        }
    }
    const QUrl url(account.value(QStringLiteral("url")).toString());
    if (!url.isValid()) return false;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml; charset=utf-8"));
    applyCalDavAuthorization(request, account.value(QStringLiteral("username")).toString(), calDavCredential(account));
    auto *network = new QNetworkAccessManager(this);
    configureCalDavAuthentication(network, account.value(QStringLiteral("username")).toString(), calDavCredential(account));
    auto *reply = network->sendCustomRequest(request, QByteArrayLiteral("ACL"), serializeCalDavPermissions(permissions));
    connect(reply, &QNetworkReply::finished, this, [this, reply, network, permissions] {
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            Q_EMIT caldavPermissionsError(reply->error() == QNetworkReply::NoError ? i18n("The CalDAV server rejected the permission changes.") : reply->errorString());
        } else {
            m_caldavPermissions = permissions;
            Q_EMIT caldavPermissionsChanged();
            Q_EMIT caldavPermissionsSaved();
        }
        reply->deleteLater();
        network->deleteLater();
    });
    return true;
}

QStringList CalendarManager::eventsForDate(const QString &date) const
{
    const auto selectedDate = QDate::fromString(date.trimmed(), Qt::ISODate);
    if (!selectedDate.isValid()) return {};

    const auto dayStart = QDateTime(selectedDate, QTime(0, 0), QTimeZone::systemTimeZone());
    const auto dayEnd = QDateTime(selectedDate.addDays(1), QTime(0, 0), QTimeZone::systemTimeZone());
    QStringList events;
    QSet<QString> seen;
    KCalendarCore::OccurrenceIterator iterator(*m_calendar, dayStart.addDays(-1), dayEnd);
    while (iterator.hasNext()) {
        iterator.next();
        const auto incidence = iterator.incidence();
        if (!incidence) continue;
        const auto start = iterator.occurrenceStartDate();
        QDateTime end = start;
        if (const auto event = incidence.dynamicCast<KCalendarCore::Event>()) end = event->dtEnd();
        if (start.date() > selectedDate || (end.isValid() && end.date() < selectedDate)) continue;
        const auto key = incidence->uid() + QLatin1Char(39) + start.toString(Qt::ISODate);
        if (seen.contains(key)) continue;
        seen.insert(key);
        QJsonObject eventData;
        eventData.insert(QStringLiteral("dateKey"), selectedDate.toString(Qt::ISODate));
        eventData.insert(QStringLiteral("start"), start.toString(Qt::ISODate));
        eventData.insert(QStringLiteral("end"), end.toString(Qt::ISODate));
        eventData.insert(QStringLiteral("title"), incidence->summary());
        eventData.insert(QStringLiteral("location"), incidence->location());
        eventData.insert(QStringLiteral("description"), incidence->description());
        eventData.insert(QStringLiteral("allDay"), incidence->allDay());
        events.append(QString::fromUtf8(QJsonDocument(eventData).toJson(QJsonDocument::Compact)));
    }
    return events;
}

QStringList CalendarManager::upcomingEvents(int days) const
{
    const auto now = QDateTime::currentDateTime();
    const auto limit = now.addDays(qMax(0, days));
    QStringList events;
    KCalendarCore::OccurrenceIterator iterator(*m_calendar, now, limit);
    while (iterator.hasNext()) {
        iterator.next();
        const auto start = iterator.occurrenceStartDate();
        const auto incidence = iterator.incidence();
        if (incidence && start.isValid())
            events.append(start.toString(Qt::ISODate) + QStringLiteral(" ") + incidence->summary());
    }
    return events;
}

KCalendarCore::MemoryCalendar::Ptr CalendarManager::calendar() const
{
    return m_calendar;
}

QColor CalendarManager::collectionColor(qint64 collectionId) const
{
    if (m_collectionColors.contains(collectionId)) return m_collectionColors.value(collectionId);
    switch (collectionId) {
    case 0:
        return QColor(0x1d, 0x99, 0xf3);
    case 1:
        return QColor(0x52, 0xb7, 0x88);
    case 2:
        return QColor(0xd8, 0x9b, 0x4a);
    default:
        return QColor::fromHsv(static_cast<int>((collectionId * 83) % 360), 160, 220);
    }
}

bool CalendarManager::setCollectionColor(qint64 collectionId, const QColor &color)
{
    if (collectionId < 0 || collectionId >= m_collections->rowCount() || !color.isValid()) return false;
    if (m_collectionColors.value(collectionId) == color) return true;
    m_collectionColors.insert(collectionId, color);
    m_collections->item(static_cast<int>(collectionId))->setData(color, Qt::UserRole + 2);
    saveCollectionColors();
    Q_EMIT calendarChanged();
    return true;
}

bool CalendarManager::isCollectionReadOnly(qint64 collectionId) const
{
    return m_readOnlyCollections.contains(collectionId) || m_serverReadOnlyCollections.contains(collectionId);
}

bool CalendarManager::setCollectionReadOnly(qint64 collectionId, bool readOnly)
{
    if (collectionId <= 0 || collectionId >= m_collections->rowCount()) return false;
    if (readOnly) m_readOnlyCollections.insert(collectionId);
    else m_readOnlyCollections.remove(collectionId);
    m_collections->item(static_cast<int>(collectionId))->setData(readOnly ? static_cast<int>(Collection::ReadOnly) : static_cast<int>(Collection::AllRights), Qt::UserRole + 3);
    saveSubscriptionMetadata();
    Q_EMIT calendarChanged();
    return true;
}

QString CalendarManager::subscriptionForCollection(qint64 collectionId) const
{
    for (auto it = m_subscriptionCollectionIds.cbegin(); it != m_subscriptionCollectionIds.cend(); ++it) {
        if (it.value() == collectionId) return it.key();
    }
    return {};
}


bool CalendarManager::startCalDavOAuthAuthorization(const QString &calendarUrl, const QString &authorizationUrl, const QString &tokenUrl, const QString &clientId, const QString &clientSecret)
{
    const QUrl authUrl(authorizationUrl);
    const QUrl tokenEndpoint(tokenUrl);
    if (calendarUrl.trimmed().isEmpty() || !authUrl.isValid() || !tokenEndpoint.isValid() || clientId.trimmed().isEmpty()) return false;
    if (!m_oauthServer) {
        m_oauthServer = new QTcpServer(this);
        connect(m_oauthServer, &QTcpServer::newConnection, this, &CalendarManager::handleOAuthConnection, Qt::UniqueConnection);
    }
    if (m_oauthServer->isListening()) return false;
    if (!m_oauthServer->listen(QHostAddress::LocalHost)) return false;

    const auto randomValue = [] {
        return QString::number(QRandomGenerator::global()->generate64(), 16) + QString::number(QRandomGenerator::global()->generate64(), 16);
    };
    m_oauthCalendarUrl = calendarUrl.trimmed();
    m_oauthAuthorizationUrl = authUrl.toString(QUrl::FullyEncoded);
    m_oauthTokenUrl = tokenEndpoint.toString(QUrl::FullyEncoded);
    m_oauthClientId = clientId.trimmed();
    m_oauthClientSecret = clientSecret;
    m_oauthState = randomValue();
    m_oauthVerifier = randomValue() + randomValue();
    const auto challenge = QCryptographicHash::hash(m_oauthVerifier.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    const auto redirectUri = QStringLiteral("http://127.0.0.1:%1/oauth/callback").arg(m_oauthServer->serverPort());

    QUrl url(authUrl);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("client_id"), m_oauthClientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    query.addQueryItem(QStringLiteral("state"), m_oauthState);
    query.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(challenge));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    url.setQuery(query);
    if (!QDesktopServices::openUrl(url)) {
        m_oauthServer->close();
        return false;
    }
    return true;
}

void CalendarManager::handleOAuthConnection()
{
    if (!m_oauthServer || !m_oauthServer->hasPendingConnections()) return;
    auto *socket = m_oauthServer->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
        if (!socket->canReadLine()) return;
        const auto requestLine = socket->readLine().trimmed();
        const auto parts = requestLine.split(' ');
        if (parts.size() < 2) {
            socket->disconnectFromHost();
            return;
        }
        const auto target = QString::fromUtf8(parts.at(1));
        const QUrl callbackUrl(QStringLiteral("http://127.0.0.1") + target);
        const QUrlQuery callbackQuery(callbackUrl);
        const auto state = callbackQuery.queryItemValue(QStringLiteral("state"));
        const auto code = callbackQuery.queryItemValue(QStringLiteral("code"));
        const auto error = callbackQuery.queryItemValue(QStringLiteral("error"));
        const QByteArray response = error.isEmpty() ? QByteArrayLiteral("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nAgenda authorization completed. You may return to Agenda.") : QByteArrayLiteral("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nAgenda authorization failed.");
        socket->write(response);
        socket->disconnectFromHost();
        socket->deleteLater();
        const auto port = m_oauthServer ? m_oauthServer->serverPort() : 0;
        if (m_oauthServer) m_oauthServer->close();
        if (!error.isEmpty() || state != m_oauthState || code.isEmpty()) {
            Q_EMIT caldavOAuthError(error.isEmpty() ? i18n("OAuth authorization returned an invalid state.") : error);
            return;
        }

        const auto calendarUrl = m_oauthCalendarUrl;
        const auto tokenUrl = m_oauthTokenUrl;
        const auto clientId = m_oauthClientId;
        const auto clientSecret = m_oauthClientSecret;
        const auto verifier = m_oauthVerifier;
        const auto redirectUri = QStringLiteral("http://127.0.0.1:%1/oauth/callback").arg(port);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
        query.addQueryItem(QStringLiteral("code"), code);
        query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
        query.addQueryItem(QStringLiteral("client_id"), clientId);
        query.addQueryItem(QStringLiteral("code_verifier"), verifier);
        if (!clientSecret.isEmpty()) query.addQueryItem(QStringLiteral("client_secret"), clientSecret);

        auto *network = new QNetworkAccessManager(this);
        QNetworkRequest request{QUrl(tokenUrl)};
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        auto *reply = network->post(request, query.query(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this, [this, reply, network, calendarUrl, tokenUrl, clientId, clientSecret] {
            const auto data = reply->readAll();
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto object = QJsonDocument::fromJson(data).object();
            const auto accessToken = object.value(QStringLiteral("access_token")).toString();
            if (reply->error() != QNetworkReply::NoError || status >= 400 || accessToken.isEmpty()) {
                Q_EMIT caldavOAuthError(i18n("OAuth token exchange failed: %1", reply->errorString()));
                reply->deleteLater();
                network->deleteLater();
                return;
            }
            QVariantMap updated;
            bool found = false;
            for (const auto &accountValue : std::as_const(m_caldavAccounts)) {
                const auto candidate = accountValue.toMap();
                if (candidate.value(QStringLiteral("url")).toString() == calendarUrl) {
                    updated = candidate;
                    found = true;
                    break;
                }
            }
            const auto collectionId = found ? updated.value(QStringLiteral("collectionId"), -1).toLongLong() : ensureCalDavCollection(calendarUrl);
            updated.insert(QStringLiteral("url"), calendarUrl);
            updated.insert(QStringLiteral("authType"), QStringLiteral("bearer"));
            updated.insert(QStringLiteral("password"), accessToken);
            updated.insert(QStringLiteral("tokenUrl"), tokenUrl);
            updated.insert(QStringLiteral("clientId"), clientId);
            updated.insert(QStringLiteral("clientSecret"), clientSecret);
            updated.insert(QStringLiteral("authorizationUrl"), m_oauthAuthorizationUrl);
            updated.insert(QStringLiteral("enabled"), true);
            updated.insert(QStringLiteral("collectionId"), collectionId);
            const auto refreshToken = object.value(QStringLiteral("refresh_token")).toString();
            if (!refreshToken.isEmpty()) updated.insert(QStringLiteral("refreshToken"), refreshToken);
            const auto expiresIn = object.value(QStringLiteral("expires_in")).toInt(3600);
            updated.insert(QStringLiteral("tokenExpiresAt"), QDateTime::currentDateTimeUtc().addSecs(qMax(60, expiresIn)));
            if (found) {
                for (auto &stored : m_caldavAccounts) {
                    if (stored.toMap().value(QStringLiteral("url")).toString() == calendarUrl) {
                        stored = updated;
                        break;
                    }
                }
            } else {
                m_caldavAccounts.append(updated);
                if (!m_caldavUrls.contains(calendarUrl)) m_caldavUrls.append(calendarUrl);
            }
            saveCalDavAccounts();
            Q_EMIT caldavAccountsChanged();
            syncCalDavAccount(updated);
            Q_EMIT caldavOAuthAuthorized();
            reply->deleteLater();
            network->deleteLater();
        });
    });
}

bool CalendarManager::saveCalDavAccount(const QString &urlString, const QString &username, const QString &secret, bool enabled, const QString &authentication, const QString &refreshToken, const QString &tokenUrl, const QString &clientId, const QString &clientSecret, const QString &authorizationUrl)
{
    const auto normalizedUrl = urlString.trimmed();
    const QUrl url(normalizedUrl);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")) || url.host().isEmpty()) return false;

    const auto authType = authentication.compare(QStringLiteral("bearer"), Qt::CaseInsensitive) == 0 ? QStringLiteral("bearer") : QStringLiteral("basic");
    QVariantMap account{{QStringLiteral("url"), normalizedUrl}, {QStringLiteral("username"), username.trimmed()}, {QStringLiteral("password"), secret}, {QStringLiteral("authType"), authType}, {QStringLiteral("refreshToken"), refreshToken}, {QStringLiteral("tokenUrl"), tokenUrl.trimmed()}, {QStringLiteral("clientId"), clientId.trimmed()}, {QStringLiteral("clientSecret"), clientSecret}, {QStringLiteral("authorizationUrl"), authorizationUrl.trimmed()}, {QStringLiteral("enabled"), enabled}};
    QString effectiveSecret = secret;
    qint64 collectionId = -1;
    bool replaced = false;
    for (const auto &stored : std::as_const(m_caldavAccounts)) {
        const auto previous = stored.toMap();
        if (previous.value(QStringLiteral("url")).toString() == normalizedUrl) {
            collectionId = previous.value(QStringLiteral("collectionId"), -1).toLongLong();
            if (secret.isEmpty()) {
                account.insert(QStringLiteral("password"), previous.value(QStringLiteral("password")));
                account.insert(QStringLiteral("tokenExpiresAt"), previous.value(QStringLiteral("tokenExpiresAt")));
                effectiveSecret = previous.value(QStringLiteral("password")).toString();
            }
            if (refreshToken.isEmpty()) account.insert(QStringLiteral("refreshToken"), previous.value(QStringLiteral("refreshToken")));
            if (tokenUrl.isEmpty()) account.insert(QStringLiteral("tokenUrl"), previous.value(QStringLiteral("tokenUrl")));
            if (clientId.isEmpty()) account.insert(QStringLiteral("clientId"), previous.value(QStringLiteral("clientId")));
            if (clientSecret.isEmpty()) account.insert(QStringLiteral("clientSecret"), previous.value(QStringLiteral("clientSecret")));
            if (authorizationUrl.isEmpty()) account.insert(QStringLiteral("authorizationUrl"), previous.value(QStringLiteral("authorizationUrl")));
            replaced = true;
            break;
        }
    }
    const auto effectiveRefreshToken = account.value(QStringLiteral("refreshToken")).toString();
    if (authType == QStringLiteral("bearer") && effectiveSecret.trimmed().isEmpty() && effectiveRefreshToken.trimmed().isEmpty()) return false;
    if (authType != QStringLiteral("bearer")) {
        account.remove(QStringLiteral("refreshToken"));
        account.remove(QStringLiteral("tokenUrl"));
        account.remove(QStringLiteral("clientId"));
        account.remove(QStringLiteral("clientSecret"));
        account.remove(QStringLiteral("authorizationUrl"));
        account.remove(QStringLiteral("tokenExpiresAt"));
    }
    if (collectionId < 0 || collectionId >= m_collectionNames.size()) collectionId = ensureCalDavCollection(normalizedUrl);
    account.insert(QStringLiteral("collectionId"), collectionId);
    if (replaced) {
        for (auto &stored : m_caldavAccounts) {
            if (stored.toMap().value(QStringLiteral("url")).toString() == normalizedUrl) {
                stored = account;
                break;
            }
        }
    } else {
        m_caldavAccounts.append(account);
    }
    if (!m_caldavUrls.contains(normalizedUrl)) m_caldavUrls.append(normalizedUrl);
    saveCalDavAccounts();
    Q_EMIT caldavAccountsChanged();
    if (enabled) syncCalDavAccount(account);
    return true;
}

bool CalendarManager::removeCalDavAccount(const QString &urlString)
{
    const auto normalizedUrl = urlString.trimmed();
    bool removed = false;
    for (int i = m_caldavAccounts.size() - 1; i >= 0; --i) {
        if (m_caldavAccounts.at(i).toMap().value(QStringLiteral("url")).toString() == normalizedUrl) {
            m_caldavAccounts.removeAt(i);
            removed = true;
        }
    }
    if (!removed) return false;
    m_caldavUrls.removeAll(normalizedUrl);
    saveCalDavAccounts();
    Q_EMIT caldavAccountsChanged();
    return true;
}

void CalendarManager::resolveSyncConflict(const QString &uid, bool keepLocal)
{
    const auto url = m_conflictUrls.value(uid);
    if (url.isEmpty()) return;
    if (keepLocal) {
        const auto etagKey = uid.isEmpty() ? url : url + QLatin1Char(124) + uid;
        m_remoteEtags.remove(etagKey);
        QSettings().remove(QStringLiteral("Agenda/caldavEtags/") + etagKey);
        for (const auto &accountValue : std::as_const(m_caldavAccounts)) {
            const auto account = accountValue.toMap();
            if (account.value(QStringLiteral("url")).toString() == url) {
                uploadCalDav(url, account.value(QStringLiteral("username")).toString(), calDavCredential(account), account.value(QStringLiteral("collectionId"), -1).toLongLong());
                break;
            }
        }
    } else if (uid.isEmpty()) {
        const auto conflicts = m_remoteConflicts;
        m_remoteConflicts.clear();
        for (auto it = conflicts.cbegin(); it != conflicts.cend(); ++it) {
            for (const auto &incidence : m_calendar->incidences()) {
                if (incidence->uid() == it.key()) m_calendar->deleteIncidence(incidence);
            }
            for (const auto &incidence : it.value()) m_calendar->addIncidence(KCalendarCore::Incidence::Ptr(incidence->clone()));
        }
        save();
        Q_EMIT calendarChanged();
    } else {
        const auto remote = m_remoteConflicts.take(uid);
        if (remote.isEmpty()) return;
        for (const auto &incidence : m_calendar->incidences()) {
            if (incidence->uid() == uid) m_calendar->deleteIncidence(incidence);
        }
        for (const auto &incidence : remote) m_calendar->addIncidence(KCalendarCore::Incidence::Ptr(incidence->clone()));
        save();
        Q_EMIT calendarChanged();
    }
    m_conflictUrls.remove(uid);
}

void CalendarManager::saveCollectionNames() const
{
    QSettings().setValue(QStringLiteral("Agenda/calendarNames"), m_collectionNames);
}

void CalendarManager::saveCollectionColors() const
{
    QStringList colors;
    for (auto it = m_collectionColors.cbegin(); it != m_collectionColors.cend(); ++it)
        colors.append(QString::number(it.key()) + QLatin1Char(9) + it.value().name(QColor::HexArgb));
    QSettings().setValue(QStringLiteral("Agenda/calendarColors"), colors);
}

void CalendarManager::saveCalDavAccounts() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("Agenda/caldavAccounts"), m_caldavAccounts);
    settings.setValue(QStringLiteral("Agenda/caldavUrls"), m_caldavUrls);
}

void CalendarManager::syncSavedCalDavAccounts()
{
    for (const auto &accountValue : std::as_const(m_caldavAccounts))
        syncCalDavAccount(accountValue.toMap());
}

void CalendarManager::syncCalDavAccount(const QVariantMap &account)
{
    if (!account.value(QStringLiteral("enabled"), true).toBool()) return;
    const auto url = account.value(QStringLiteral("url")).toString();
    if (url.isEmpty()) return;
    const auto authType = account.value(QStringLiteral("authType"), QStringLiteral("basic")).toString();
    if (authType != QStringLiteral("bearer")) {
        syncCalDav(url, account.value(QStringLiteral("username")).toString(), calDavCredential(account), account.value(QStringLiteral("collectionId"), -1).toLongLong());
        return;
    }
    const auto token = account.value(QStringLiteral("password")).toString();
    const auto expiresAt = account.value(QStringLiteral("tokenExpiresAt")).toDateTime();
    const auto refreshToken = account.value(QStringLiteral("refreshToken")).toString();
    if (!token.isEmpty() && (!expiresAt.isValid() || expiresAt > QDateTime::currentDateTimeUtc().addSecs(60))) {
        syncCalDav(url, account.value(QStringLiteral("username")).toString(), calDavCredential(account), account.value(QStringLiteral("collectionId"), -1).toLongLong());
    } else if (!refreshToken.isEmpty() && !account.value(QStringLiteral("tokenUrl")).toString().isEmpty()) {
        refreshCalDavToken(account);
    } else if (!token.isEmpty()) {
        syncCalDav(url, account.value(QStringLiteral("username")).toString(), calDavCredential(account), account.value(QStringLiteral("collectionId"), -1).toLongLong());
    }
}

void CalendarManager::refreshCalDavToken(const QVariantMap &account)
{
    const auto tokenUrl = QUrl(account.value(QStringLiteral("tokenUrl")).toString());
    const auto refreshToken = account.value(QStringLiteral("refreshToken")).toString();
    if (!tokenUrl.isValid() || refreshToken.isEmpty()) return;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    query.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    if (!account.value(QStringLiteral("clientId")).toString().isEmpty()) query.addQueryItem(QStringLiteral("client_id"), account.value(QStringLiteral("clientId")).toString());
    if (!account.value(QStringLiteral("clientSecret")).toString().isEmpty()) query.addQueryItem(QStringLiteral("client_secret"), account.value(QStringLiteral("clientSecret")).toString());

    auto *network = new QNetworkAccessManager(this);
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    auto *reply = network->post(request, query.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, network, account] {
        const auto data = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto object = QJsonDocument::fromJson(data).object();
        const auto accessToken = object.value(QStringLiteral("access_token")).toString();
        if (reply->error() != QNetworkReply::NoError || status >= 400 || accessToken.isEmpty()) {
            qWarning() << "OAuth token refresh failed:" << reply->errorString() << status;
            reply->deleteLater();
            network->deleteLater();
            return;
        }
        QVariantMap updated = account;
        updated.insert(QStringLiteral("password"), accessToken);
        const auto newRefreshToken = object.value(QStringLiteral("refresh_token")).toString();
        if (!newRefreshToken.isEmpty()) updated.insert(QStringLiteral("refreshToken"), newRefreshToken);
        const auto expiresIn = object.value(QStringLiteral("expires_in")).toInt(3600);
        updated.insert(QStringLiteral("tokenExpiresAt"), QDateTime::currentDateTimeUtc().addSecs(qMax(60, expiresIn)));
        for (auto &stored : m_caldavAccounts) {
            if (stored.toMap().value(QStringLiteral("url")).toString() == updated.value(QStringLiteral("url")).toString()) {
                stored = updated;
                break;
            }
        }
        saveCalDavAccounts();
        Q_EMIT caldavAccountsChanged();
        syncCalDav(updated.value(QStringLiteral("url")).toString(), updated.value(QStringLiteral("username")).toString(), calDavCredential(updated), updated.value(QStringLiteral("collectionId"), -1).toLongLong());
        reply->deleteLater();
        network->deleteLater();
    });
}

void CalendarManager::setServerCollectionReadOnly(qint64 collectionId, bool readOnly)
{
    if (collectionId < 0 || !m_collections || collectionId >= m_collections->rowCount()) return;
    if (readOnly) m_serverReadOnlyCollections.insert(collectionId);
    else m_serverReadOnlyCollections.remove(collectionId);
    m_collections->item(static_cast<int>(collectionId))->setData(isCollectionReadOnly(collectionId) ? static_cast<int>(Collection::ReadOnly) : static_cast<int>(Collection::AllRights), Qt::UserRole + 3);
    Q_EMIT calendarChanged();
}

void CalendarManager::saveSubscriptionMetadata() const
{
    QStringList readOnlyIds;
    for (const auto id : m_readOnlyCollections) readOnlyIds.append(QString::number(id));
    QStringList subscriptionIds;
    for (auto it = m_subscriptionCollectionIds.cbegin(); it != m_subscriptionCollectionIds.cend(); ++it)
        subscriptionIds.append(it.key() + QLatin1Char(9) + QString::number(it.value()));
    QSettings settings;
    settings.setValue(QStringLiteral("Agenda/readOnlyCollections"), readOnlyIds);
    settings.setValue(QStringLiteral("Agenda/subscriptionCollectionIds"), subscriptionIds);
}

void CalendarManager::saveRemoteState(const QString &url, const QSet<QString> &uids) const
{
    QStringList values;
    for (const auto &uid : uids) values.append(uid);
    QSettings().setValue(QStringLiteral("Agenda/caldavUids/") + url, values);
}

QSet<QString> CalendarManager::remoteState(const QString &url) const
{
    QSet<QString> result;
    const auto values = QSettings().value(QStringLiteral("Agenda/caldavUids/") + url).toStringList();
    for (const auto &uid : values) result.insert(uid);
    return result;
}

void CalendarManager::saveDeletedRemoteState(const QString &url, const QSet<QString> &uids) const
{
    QStringList values;
    for (const auto &uid : uids) values.append(uid);
    QSettings().setValue(QStringLiteral("Agenda/caldavDeleted/") + url, values);
}

QSet<QString> CalendarManager::deletedRemoteState(const QString &url) const
{
    QSet<QString> result;
    const auto values = QSettings().value(QStringLiteral("Agenda/caldavDeleted/") + url).toStringList();
    for (const auto &uid : values) result.insert(uid);
    return result;
}

void CalendarManager::markRemoteDeletion(const QString &uid, qint64 collectionId)
{
    if (uid.isEmpty()) return;
    for (const auto &url : m_caldavUrls) {
        const auto knownUids = remoteState(url);
        if (!knownUids.contains(uid)) continue;
        if (collectionId >= 0) {
            bool belongsToCollection = false;
            for (const auto &accountValue : std::as_const(m_caldavAccounts)) {
                const auto account = accountValue.toMap();
                if (account.value(QStringLiteral("url")).toString() == url && account.value(QStringLiteral("collectionId"), -1).toLongLong() == collectionId) {
                    belongsToCollection = true;
                    break;
                }
            }
            if (!belongsToCollection) continue;
        }
        auto deletedUids = deletedRemoteState(url);
        deletedUids.insert(uid);
        saveDeletedRemoteState(url, deletedUids);
    }
}

void CalendarManager::deleteCalDavResources(const QString &urlString, const QStringList &uids, const QString &username, const QString &password)
{
    const QUrl calendarUrl(urlString);
    if (!calendarUrl.isValid() || calendarUrl.path().endsWith(QStringLiteral(".ics")) || uids.isEmpty()) return;

    auto *network = new QNetworkAccessManager(this);
    configureCalDavAuthentication(network, username, password);
    auto remaining = QSharedPointer<int>::create(uids.size());
    for (const auto &uid : uids) {
        QUrl eventUrl = calendarUrl;
        auto eventPath = eventUrl.path();
        if (!eventPath.endsWith(QLatin1Char(47))) eventPath.append(QLatin1Char(47));
        eventUrl.setPath(eventPath + uid + QStringLiteral(".ics"));
        QNetworkRequest request(eventUrl);
        const auto etagKey = urlString + QLatin1Char(124) + uid;
        if (!m_remoteEtags.value(etagKey).isEmpty()) request.setRawHeader("If-Match", m_remoteEtags.value(etagKey));
        applyCalDavAuthorization(request, username, password);
        auto *reply = network->deleteResource(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, network, remaining, urlString, uid, etagKey] {
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 409 || status == 412) {
                Q_EMIT syncConflict(uid);
            } else if (reply->error() == QNetworkReply::NoError || status == 404) {
                auto deletedUids = deletedRemoteState(urlString);
                deletedUids.remove(uid);
                saveDeletedRemoteState(urlString, deletedUids);
                m_remoteEtags.remove(etagKey);
                QSettings().remove(QStringLiteral("Agenda/caldavEtags/") + etagKey);
            } else {
                qWarning() << "CalDAV deletion failed:" << reply->errorString();
            }
            reply->deleteLater();
            if (--(*remaining) == 0) network->deleteLater();
        });
    }
}

qint64 CalendarManager::ensureCalDavCollection(const QString &url)
{
    const QUrl parsedUrl(url);
    const auto host = parsedUrl.host().isEmpty() ? url : parsedUrl.host();
    auto name = i18n("CalDAV: %1", host);
    for (int suffix = 2; m_collectionNames.contains(name, Qt::CaseInsensitive); ++suffix)
        name = i18n("CalDAV: %1 (%2)", host, suffix);

    const auto collectionId = m_collectionNames.size();
    m_collectionNames.append(name);
    auto item = new QStandardItem(name);
    item->setCheckable(true);
    item->setCheckState(Qt::Checked);
    item->setData(collectionId, Qt::UserRole + 1);
    item->setData(collectionColor(collectionId), Qt::UserRole + 2);
    item->setData(static_cast<int>(Collection::AllRights), Qt::UserRole + 3);
    m_collections->appendRow(item);
    saveCollectionNames();
    return collectionId;
}

qint64 CalendarManager::ensureSubscriptionCollection(const QString &url)
{
    if (m_subscriptionCollectionIds.contains(url) && m_subscriptionCollectionIds.value(url) < m_collectionNames.size()) {
        const auto collectionId = m_subscriptionCollectionIds.value(url);
        m_readOnlyCollections.insert(collectionId);
        if (m_collections && collectionId < m_collections->rowCount())
            m_collections->item(static_cast<int>(collectionId))->setData(static_cast<int>(Collection::ReadOnly), Qt::UserRole + 3);
        return collectionId;
    }

    const QUrl parsedUrl(url);
    const auto host = parsedUrl.host().isEmpty() ? url : parsedUrl.host();
    auto name = i18n("Subscription: %1", host);
    for (int suffix = 2; m_collectionNames.contains(name, Qt::CaseInsensitive); ++suffix)
        name = i18n("Subscription: %1 (%2)", host, suffix);

    const auto collectionId = m_collectionNames.size();
    m_collectionNames.append(name);
    m_subscriptionCollectionIds.insert(url, collectionId);
    m_readOnlyCollections.insert(collectionId);

    auto item = new QStandardItem(name);
    item->setCheckable(true);
    item->setCheckState(Qt::Checked);
    item->setData(collectionId, Qt::UserRole + 1);
    item->setData(collectionColor(collectionId), Qt::UserRole + 2);
    item->setData(static_cast<int>(Collection::ReadOnly), Qt::UserRole + 3);
    m_collections->appendRow(item);
    saveCollectionNames();
    saveSubscriptionMetadata();
    return collectionId;
}

bool CalendarManager::addCollection(const QString &name)
{
    const auto trimmedName = name.trimmed();
    if (trimmedName.isEmpty() || m_collectionNames.contains(trimmedName, Qt::CaseInsensitive)) return false;
    const auto collectionId = m_collectionNames.size();
    m_collectionNames.append(trimmedName);
    auto item = new QStandardItem(trimmedName);
    item->setCheckable(true);
    item->setCheckState(Qt::Checked);
    item->setData(collectionId, Qt::UserRole + 1);
    item->setData(collectionColor(collectionId), Qt::UserRole + 2);
    item->setData(static_cast<int>(Collection::AllRights), Qt::UserRole + 3);
    m_collections->appendRow(item);
    saveCollectionNames();
    return true;
}

bool CalendarManager::removeCollection(qint64 collectionId)
{
    if (collectionId <= 0 || collectionId >= m_collectionNames.size() || subscriptionForCollection(collectionId).size() > 0 || isCollectionReadOnly(collectionId)) return false;
    for (const auto &accountValue : std::as_const(m_caldavAccounts)) {
        if (accountValue.toMap().value(QStringLiteral("collectionId"), -1).toLongLong() == collectionId) return false;
    }

    for (const auto &incidence : m_calendar->incidences()) {
        const auto incidenceCollectionId = incidence->customProperty("MauiCalendar", "CollectionId").toLongLong();
        if (incidenceCollectionId == collectionId)
            incidence->setCustomProperty("MauiCalendar", "CollectionId", QStringLiteral("0"));
        else if (incidenceCollectionId > collectionId)
            incidence->setCustomProperty("MauiCalendar", "CollectionId", QString::number(incidenceCollectionId - 1));
    }

    m_collectionNames.removeAt(static_cast<int>(collectionId));
    QHash<qint64, QColor> shiftedColors;
    for (auto it = m_collectionColors.cbegin(); it != m_collectionColors.cend(); ++it) {
        if (it.key() < collectionId) shiftedColors.insert(it.key(), it.value());
        else if (it.key() > collectionId) shiftedColors.insert(it.key() - 1, it.value());
    }
    m_collectionColors = shiftedColors;
    m_collections->removeRow(static_cast<int>(collectionId));
    for (int i = static_cast<int>(collectionId); i < m_collections->rowCount(); ++i) {
        auto *item = m_collections->item(i);
        item->setData(i, Qt::UserRole + 1);
        item->setData(collectionColor(i), Qt::UserRole + 2);
    }

    QSet<qint64> shiftedReadOnlyCollections;
    for (const auto id : m_readOnlyCollections) {
        if (id < collectionId) shiftedReadOnlyCollections.insert(id);
        else if (id > collectionId) shiftedReadOnlyCollections.insert(id - 1);
    }
    m_readOnlyCollections = shiftedReadOnlyCollections;

    QHash<QString, qint64> shiftedSubscriptions;
    for (auto it = m_subscriptionCollectionIds.cbegin(); it != m_subscriptionCollectionIds.cend(); ++it)
        shiftedSubscriptions.insert(it.key(), it.value() > collectionId ? it.value() - 1 : it.value());
    m_subscriptionCollectionIds = shiftedSubscriptions;

    saveCollectionNames();
    saveCollectionColors();
    saveSubscriptionMetadata();
    save();
    Q_EMIT calendarChanged();
    return true;
}

KCalendarCore::Incidence::List CalendarManager::childIncidences(const QString &uid) const
{
    KCalendarCore::Incidence::List children;
    const auto all = m_calendar->incidences();
    for (const auto &inc : all) {
        if (inc->relatedTo() == uid) {
            children.append(inc);
        }
    }
    return children;
}

void CalendarManager::save()
{
    if (!m_storage) {
        return;
    }
    for (const auto &incidence : m_calendar->incidences()) normalizeIncidenceTimes(incidence);
    if (!m_storage->save()) {
        qWarning() << "Failed to save calendar";
    }
}

void CalendarManager::mergeCalendar(const KCalendarCore::MemoryCalendar::Ptr &source, bool preferRemote, qint64 collectionId)
{
    bool changed = false;
    QSet<QString> incomingUids;
    for (const auto &incidence : source->incidences()) {
        auto incoming = KCalendarCore::Incidence::Ptr(incidence->clone());
        normalizeIncidenceTimes(incoming);
        if (collectionId >= 0)
            incoming->setCustomProperty("MauiCalendar", "CollectionId", QString::number(collectionId));
        else if (incoming->customProperty("MauiCalendar", "CollectionId").isEmpty())
            incoming->setCustomProperty("MauiCalendar", "CollectionId", QStringLiteral("0"));
        incomingUids.insert(incoming->uid());
        KCalendarCore::Incidence::Ptr existing;
        const auto incomingCollectionId = incoming->customProperty("MauiCalendar", "CollectionId");
        for (const auto &candidate : m_calendar->incidences()) {
            if (candidate->uid() != incoming->uid()) continue;
            if (candidate->customProperty("MauiCalendar", "CollectionId") != incomingCollectionId) continue;
            if (incoming->hasRecurrenceId() && (!candidate->hasRecurrenceId() || candidate->recurrenceId() != incoming->recurrenceId())) continue;
            if (!incoming->hasRecurrenceId() && candidate->hasRecurrenceId()) continue;
            existing = candidate;
            break;
        }
        if (!existing) {
            m_calendar->addIncidence(incoming);
            changed = true;
            continue;
        }
        const auto incomingModified = incoming->lastModified();
        const auto existingModified = existing->lastModified();
        const bool subscriptionUpdate = collectionId >= 0 && preferRemote;
        if (subscriptionUpdate || (preferRemote && incomingModified.isValid() && (!existingModified.isValid() || incomingModified > existingModified))) {
            m_calendar->deleteIncidence(existing);
            m_calendar->addIncidence(incoming);
            changed = true;
        }
    }

    if (collectionId >= 0) {
        const auto existingIncidences = m_calendar->incidences();
        for (const auto &existing : existingIncidences) {
            if (existing->customProperty("MauiCalendar", "CollectionId").toLongLong() == collectionId && !incomingUids.contains(existing->uid())) {
                m_calendar->deleteIncidence(existing);
                changed = true;
            }
        }
    }

    if (changed) {
        save();
        Q_EMIT calendarChanged();
    }
}

bool CalendarManager::importCalendarData(const QByteArray &data, bool preferRemote, qint64 collectionId)
{
    if (data.isEmpty()) return false;
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/agenda-XXXXXX.ics"));
    file.setAutoRemove(false);
    if (!file.open()) return false;
    file.write(data);
    const auto path = file.fileName();
    file.close();
    const auto importedCalendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());
    KCalendarCore::FileStorage importedStorage(importedCalendar, path);
    const bool loaded = importedStorage.open() && importedStorage.load();
    if (loaded) mergeCalendar(importedCalendar, preferRemote, collectionId);
    QFile::remove(path);
    return loaded;
}

QSet<QString> CalendarManager::calendarUids(const QByteArray &data) const
{
    QSet<QString> result;
    if (data.isEmpty()) return result;
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/agenda-XXXXXX.ics"));
    file.setAutoRemove(false);
    if (!file.open()) return result;
    file.write(data);
    const auto path = file.fileName();
    file.close();
    const auto importedCalendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());
    KCalendarCore::FileStorage importedStorage(importedCalendar, path);
    if (importedStorage.open() && importedStorage.load()) {
        for (const auto &incidence : importedCalendar->incidences()) result.insert(incidence->uid());
    }
    QFile::remove(path);
    return result;
}

KCalendarCore::Incidence::List CalendarManager::calendarIncidences(const QByteArray &data) const
{
    KCalendarCore::Incidence::List result;
    if (data.isEmpty()) return result;
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/agenda-XXXXXX.ics"));
    file.setAutoRemove(false);
    if (!file.open()) return result;
    file.write(data);
    const auto path = file.fileName();
    file.close();
    const auto importedCalendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());
    KCalendarCore::FileStorage importedStorage(importedCalendar, path);
    if (importedStorage.open() && importedStorage.load()) {
        for (const auto &incidence : importedCalendar->incidences()) {
            auto clone = KCalendarCore::Incidence::Ptr(incidence->clone());
            normalizeIncidenceTimes(clone);
            result.append(clone);
        }
    }
    QFile::remove(path);
    return result;
}

bool CalendarManager::importFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }

    const auto importedCalendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());
    KCalendarCore::FileStorage importedStorage(importedCalendar, filePath);
    if (!importedStorage.open() || !importedStorage.load()) {
        qWarning() << "Failed to import calendar file:" << filePath;
        return false;
    }

    mergeCalendar(importedCalendar, false, -1);
    return true;
}

void CalendarManager::importUrl(const QString &urlString)
{
    importUrl(urlString, m_subscriptionCollectionIds.value(urlString, -1));
}

void CalendarManager::importUrl(const QString &urlString, qint64 collectionId)
{
    auto url = QUrl(urlString);
    if (url.scheme() == QStringLiteral("webcal")) url.setScheme(QStringLiteral("https"));
    if (!url.isValid() || url.scheme().isEmpty()) return;

    auto *network = new QNetworkAccessManager(this);
    auto *reply = network->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, network, collectionId] {
        const auto data = reply->readAll();
        if (reply->error() == QNetworkReply::NoError && !data.isEmpty()) {
            importCalendarData(data, true, collectionId);
        } else {
            qWarning() << "Failed to download calendar URL:" << reply->errorString();
        }
        reply->deleteLater();
        network->deleteLater();
    });
}

void CalendarManager::subscribeUrl(const QString &url)
{
    const auto normalizedUrl = url.trimmed();
    if (normalizedUrl.isEmpty() || m_subscriptions.contains(normalizedUrl)) return;
    const auto collectionId = ensureSubscriptionCollection(normalizedUrl);
    m_subscriptions.append(normalizedUrl);
    QSettings().setValue(QStringLiteral("Agenda/calendarSubscriptions"), m_subscriptions);
    Q_EMIT subscriptionsChanged();
    importUrl(normalizedUrl, collectionId);
}

void CalendarManager::unsubscribeUrl(const QString &url)
{
    const auto normalizedUrl = url.trimmed();
    if (!m_subscriptions.removeOne(normalizedUrl)) return;

    const auto hasCollection = m_subscriptionCollectionIds.contains(normalizedUrl);
    const auto collectionId = m_subscriptionCollectionIds.take(normalizedUrl);
    m_readOnlyCollections.remove(collectionId);
    if (hasCollection && collectionId >= 0) {
        for (const auto &incidence : m_calendar->incidences()) {
            if (incidence->customProperty("MauiCalendar", "CollectionId").toLongLong() == collectionId)
                m_calendar->deleteIncidence(incidence);
        }
        removeCollection(collectionId);
    }

    QSettings().setValue(QStringLiteral("Agenda/calendarSubscriptions"), m_subscriptions);
    saveSubscriptionMetadata();
    Q_EMIT subscriptionsChanged();
}

void CalendarManager::uploadCalDav(const QString &urlString, const QString &username, const QString &password, qint64 collectionId)
{
    const QUrl calendarUrl(urlString);
    if (!calendarUrl.isValid()) return;

    struct Upload { QUrl url; QByteArray payload; QString uid; };
    QList<Upload> uploads;
    const bool resourceUrl = calendarUrl.path().endsWith(QStringLiteral(".ics"));
    if (resourceUrl) {
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/agenda-XXXXXX.ics"));
        file.setAutoRemove(false);
        if (!file.open()) return;
        const auto path = file.fileName();
        file.close();
        const auto writableCalendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());
        for (const auto &incidence : m_calendar->incidences()) {
            const auto incidenceCollectionId = incidence->customProperty("MauiCalendar", "CollectionId").toLongLong();
            if ((collectionId < 0 || incidenceCollectionId == collectionId) && !isCollectionReadOnly(incidenceCollectionId))
                writableCalendar->addIncidence(incidenceForSerialization(incidence));
        }
        KCalendarCore::FileStorage writableStorage(writableCalendar, path);
        if (!writableStorage.save()) { QFile::remove(path); return; }
        QFile payloadFile(path);
        const auto payload = payloadFile.open(QIODevice::ReadOnly) ? payloadFile.readAll() : QByteArray();
        payloadFile.close();
        QFile::remove(path);
        if (!payload.isEmpty()) uploads.append({calendarUrl, payload, QString()});
    } else {
        QHash<QString, KCalendarCore::Incidence::List> incidencesByUid;
        for (const auto &incidence : m_calendar->incidences()) {
            const auto incidenceCollectionId = incidence->customProperty("MauiCalendar", "CollectionId").toLongLong();
            if ((collectionId >= 0 && incidenceCollectionId != collectionId) || isCollectionReadOnly(incidenceCollectionId)) continue;
            incidencesByUid[incidence->uid()].append(incidence);
        }

        for (auto it = incidencesByUid.cbegin(); it != incidencesByUid.cend(); ++it) {
            const auto oneCalendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());
            for (const auto &incidence : it.value())
                oneCalendar->addIncidence(incidenceForSerialization(incidence));
            QTemporaryFile file(QDir::tempPath() + QStringLiteral("/agenda-XXXXXX.ics"));
            file.setAutoRemove(false);
            if (!file.open()) continue;
            const auto path = file.fileName();
            file.close();
            KCalendarCore::FileStorage storage(oneCalendar, path);
            if (!storage.save()) { QFile::remove(path); continue; }
            QFile payloadFile(path);
            const auto payload = payloadFile.open(QIODevice::ReadOnly) ? payloadFile.readAll() : QByteArray();
            payloadFile.close();
            QFile::remove(path);
            if (payload.isEmpty()) continue;
            QUrl eventUrl = calendarUrl;
            auto eventPath = eventUrl.path();
            if (!eventPath.endsWith(QLatin1Char(47))) eventPath.append(QLatin1Char(47));
            eventUrl.setPath(eventPath + it.key() + QStringLiteral(".ics"));
            uploads.append({eventUrl, payload, it.key()});
        }
    }

    if (uploads.isEmpty()) return;
    auto *network = new QNetworkAccessManager(this);
    configureCalDavAuthentication(network, username, password);
    auto remaining = QSharedPointer<int>::create(uploads.size());
    for (const auto &upload : uploads) {
        QNetworkRequest request(upload.url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/calendar; charset=utf-8"));
        const auto etagKey = upload.uid.isEmpty() ? urlString : urlString + QLatin1Char(124) + upload.uid;
        if (!m_remoteEtags.value(etagKey).isEmpty()) request.setRawHeader("If-Match", m_remoteEtags.value(etagKey));
        applyCalDavAuthorization(request, username, password);
        auto *reply = network->put(request, upload.payload);
        connect(reply, &QNetworkReply::finished, this, [this, reply, network, remaining, uid = upload.uid, etagKey, urlString, collectionId] {
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 409 || status == 412) {
                m_conflictUrls.insert(uid, urlString);
                Q_EMIT syncConflict(uid);
            } else if (status == 403 || status == 405) {
                if (collectionId >= 0) setServerCollectionReadOnly(collectionId, true);
            } else if (reply->error() != QNetworkReply::NoError) qWarning() << "CalDAV upload failed:" << reply->errorString();
            if (collectionId >= 0 && status >= 200 && status < 300) setServerCollectionReadOnly(collectionId, false);
            const auto etag = reply->rawHeader("ETag");
            if (!etag.isEmpty()) {
                m_remoteEtags.insert(etagKey, etag);
                QSettings().setValue(QStringLiteral("Agenda/caldavEtags/") + etagKey, etag);
            }
            reply->deleteLater();
            if (--(*remaining) == 0) network->deleteLater();
        });
    }
}

void CalendarManager::syncCalDav(const QString &urlString, const QString &username, const QString &password, qint64 collectionId)
{
    const QUrl url(urlString);
    const auto path = url.path();
    if (url.isValid() && (path.isEmpty() || path == QStringLiteral("/")) && !m_caldavDiscoveryUrls.contains(urlString)) {
        m_caldavDiscoveryUrls.insert(urlString);
        startCalDavDiscovery(urlString, urlString, username, password, collectionId);
        return;
    }
    syncCalDavEndpoint(urlString, username, password, collectionId);
}

void CalendarManager::startCalDavDiscovery(const QString &requestUrl, const QString &originalUrl, const QString &username, const QString &password, qint64 collectionId)
{
    const QUrl url(requestUrl);
    if (!url.isValid() || url.scheme().isEmpty()) {
        m_caldavDiscoveryUrls.remove(originalUrl);
        syncCalDavEndpoint(originalUrl, username, password, collectionId);
        return;
    }
    QNetworkRequest request(url);
    request.setRawHeader("Depth", "1");
    request.setRawHeader("Content-Type", "application/xml; charset=utf-8");
    applyCalDavAuthorization(request, username, password);
    const QByteArray propfind = QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\" ?><d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\"><d:prop><d:resourcetype/><d:current-user-principal/><c:calendar-home-set/></d:prop></d:propfind>");
    auto *network = new QNetworkAccessManager(this);
    configureCalDavAuthentication(network, username, password);
    auto *reply = network->sendCustomRequest(request, QByteArrayLiteral("PROPFIND"), propfind);
    connect(reply, &QNetworkReply::finished, this, [this, reply, network, url, originalUrl, username, password, collectionId] {
        const auto result = parseCalDavDiscovery(reply->readAll(), url);
        if (result.calendars.isEmpty() && result.calendarHome.isValid() && result.calendarHome != url) {
            const auto homeUrl = result.calendarHome.toString(QUrl::FullyEncoded);
            reply->deleteLater();
            network->deleteLater();
            startCalDavDiscovery(homeUrl, originalUrl, username, password, collectionId);
            return;
        }
        if (result.calendars.isEmpty() && !result.calendarHome.isValid() && result.principal.isValid() && result.principal != url) {
            const auto principalUrl = result.principal.toString(QUrl::FullyEncoded);
            reply->deleteLater();
            network->deleteLater();
            startCalDavDiscovery(principalUrl, originalUrl, username, password, collectionId);
            return;
        }

        auto discoveredUrls = result.calendars;
        if (discoveredUrls.isEmpty() && result.calendarHome.isValid()) discoveredUrls.append(result.calendarHome.toString(QUrl::FullyEncoded));
        if (discoveredUrls.isEmpty()) discoveredUrls.append(originalUrl);
        const auto primaryUrl = discoveredUrls.constFirst();
        if (primaryUrl != originalUrl) {
            for (auto &stored : m_caldavAccounts) {
                auto account = stored.toMap();
                if (account.value(QStringLiteral("url")).toString() == originalUrl) {
                    account.insert(QStringLiteral("url"), primaryUrl);
                    stored = account;
                    break;
                }
            }
            m_caldavUrls.removeAll(originalUrl);
        }
        for (const auto &endpoint : discoveredUrls) {
            if (!m_caldavUrls.contains(endpoint)) m_caldavUrls.append(endpoint);
        }
        saveCalDavAccounts();
        Q_EMIT caldavAccountsChanged();
        m_caldavDiscoveryUrls.remove(originalUrl);
        reply->deleteLater();
        network->deleteLater();
        for (int i = 0; i < discoveredUrls.size(); ++i)
            syncCalDavEndpoint(discoveredUrls.at(i), username, password, i == 0 ? collectionId : -1);
    });
}

void CalendarManager::syncCalDavEndpoint(const QString &urlString, const QString &username, const QString &password, qint64 collectionId)
{
    const QUrl url(urlString);
    if (!url.isValid() || url.scheme().isEmpty()) return;
    const bool bearerCredential = password.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive);
    const auto storedSecret = bearerCredential ? password.mid(7).trimmed() : password;
    const auto authType = bearerCredential ? QStringLiteral("bearer") : QStringLiteral("basic");
    if (collectionId < 0) {
        for (const auto &accountValue : std::as_const(m_caldavAccounts)) {
            const auto account = accountValue.toMap();
            if (account.value(QStringLiteral("url")).toString() == urlString) {
                collectionId = account.value(QStringLiteral("collectionId"), -1).toLongLong();
                break;
            }
        }
    }
    bool accountChanged = false;
    bool accountFound = false;
    for (auto &stored : m_caldavAccounts) {
        auto account = stored.toMap();
        if (account.value(QStringLiteral("url")).toString() == urlString) {
            accountFound = true;
            if (collectionId < 0) collectionId = account.value(QStringLiteral("collectionId"), -1).toLongLong();
            if (collectionId < 0) {
                collectionId = ensureCalDavCollection(urlString);
                account.insert(QStringLiteral("collectionId"), collectionId);
                accountChanged = true;
            }
            if (account.value(QStringLiteral("username")).toString() != username || account.value(QStringLiteral("password")).toString() != storedSecret || account.value(QStringLiteral("authType"), QStringLiteral("basic")).toString() != authType) {
                account.insert(QStringLiteral("username"), username);
                account.insert(QStringLiteral("password"), storedSecret);
                account.insert(QStringLiteral("authType"), authType);
                accountChanged = true;
            }
            stored = account;
            break;
        }
    }
    if (!accountFound) {
        if (collectionId < 0) collectionId = ensureCalDavCollection(urlString);
        m_caldavAccounts.append(QVariantMap{{QStringLiteral("url"), urlString}, {QStringLiteral("username"), username}, {QStringLiteral("password"), storedSecret}, {QStringLiteral("authType"), authType}, {QStringLiteral("enabled"), true}, {QStringLiteral("collectionId"), collectionId}});
        accountChanged = true;
    }
    const bool urlAdded = !m_caldavUrls.contains(urlString);
    if (urlAdded) m_caldavUrls.append(urlString);
    if (accountChanged || urlAdded) {
        saveCalDavAccounts();
        Q_EMIT caldavAccountsChanged();
    }
    m_remoteEtags.insert(urlString, QSettings().value(QStringLiteral("Agenda/caldavEtags/") + urlString).toByteArray());

    QNetworkRequest request(url);
    request.setRawHeader("Depth", "1");
    request.setRawHeader("Content-Type", "application/xml; charset=utf-8");
    request.setRawHeader("Accept", "text/calendar, application/xml");
    applyCalDavAuthorization(request, username, password);
    const QByteArray report = QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\" ?><c:calendar-query xmlns:c=\"urn:ietf:params:xml:ns:caldav\" xmlns:d=\"DAV:\"><d:prop><d:getetag/><d:current-user-privilege-set/><c:calendar-data/></d:prop><c:filter><c:comp-filter name=\"VCALENDAR\"><c:comp-filter name=\"VEVENT\"/></c:comp-filter></c:filter></c:calendar-query>");

    auto *network = new QNetworkAccessManager(this);
    configureCalDavAuthentication(network, username, password);
    const bool resourceUrl = url.path().endsWith(QStringLiteral(".ics"));
    auto *reply = resourceUrl ? network->get(request) : network->sendCustomRequest(request, QByteArrayLiteral("REPORT"), report);
    connect(reply, &QNetworkReply::finished, this, [this, reply, network, urlString, username, password, resourceUrl, collectionId] {
        const auto data = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = reply->error() == QNetworkReply::NoError && status < 400;
        bool parsed = false;
        QSet<QString> remoteUids;
        if (!success) {
            qWarning() << "CalDAV sync failed:" << reply->errorString() << status;
            if ((status == 403 || status == 405) && collectionId >= 0) setServerCollectionReadOnly(collectionId, true);
        } else {
            const auto etag = reply->rawHeader("ETag");
            if (!etag.isEmpty()) {
                m_remoteEtags.insert(urlString, etag);
                QSettings().setValue(QStringLiteral("Agenda/caldavEtags/") + urlString, etag);
            }

            if (resourceUrl && data.contains("BEGIN:VCALENDAR")) {
                parsed = importCalendarData(data, true, collectionId);
                remoteUids = calendarUids(data);
                for (const auto &incidence : calendarIncidences(data)) {
                    if (collectionId >= 0) incidence->setCustomProperty("MauiCalendar", "CollectionId", QString::number(collectionId));
                    m_remoteConflicts[incidence->uid()].clear();
                    m_remoteConflicts[incidence->uid()].append(incidence);
                }
            } else if (!resourceUrl) {
                QXmlStreamReader xml(data);
                QByteArray calendarData;
                QByteArray resourceEtag;
                bool inCalendarData = false;
                bool inEtag = false;
                bool inPrivilegeSet = false;
                bool privilegeSetSeen = false;
                bool serverWritable = false;
                while (!xml.atEnd()) {
                    xml.readNext();
                    if (xml.isStartElement()) {
                        if (xml.name() == QLatin1String("current-user-privilege-set")) {
                            inPrivilegeSet = true;
                            privilegeSetSeen = true;
                        } else if (inPrivilegeSet && (xml.name() == QLatin1String("write") || xml.name() == QLatin1String("write-content") || xml.name() == QLatin1String("write-properties") || xml.name() == QLatin1String("bind") || xml.name() == QLatin1String("unbind"))) {
                            serverWritable = true;
                        } else if (xml.name() == QLatin1String("calendar-data")) {
                            inCalendarData = true;
                            calendarData.clear();
                        } else if (xml.name() == QLatin1String("getetag")) {
                            inEtag = true;
                            resourceEtag.clear();
                        }
                    } else if (xml.isCharacters()) {
                        if (inCalendarData) calendarData.append(xml.text().toString().toUtf8());
                        if (inEtag) resourceEtag.append(xml.text().toString().toUtf8());
                    } else if (xml.isEndElement()) {
                        if (xml.name() == QLatin1String("calendar-data")) {
                            if (calendarData.contains("BEGIN:VCALENDAR")) {
                                parsed = importCalendarData(calendarData, true, collectionId) || parsed;
                                const auto uids = calendarUids(calendarData);
                                const auto remoteIncidences = calendarIncidences(calendarData);
                                for (const auto &incidence : remoteIncidences) {
                                    if (collectionId >= 0) incidence->setCustomProperty("MauiCalendar", "CollectionId", QString::number(collectionId));
                                    if (!remoteUids.contains(incidence->uid())) m_remoteConflicts[incidence->uid()].clear();
                                    m_remoteConflicts[incidence->uid()].append(incidence);
                                }
                                remoteUids.unite(uids);
                                for (const auto &uid : uids) {
                                    if (!resourceEtag.isEmpty()) {
                                        const auto etagKey = urlString + QLatin1Char(124) + uid;
                                        m_remoteEtags.insert(etagKey, resourceEtag);
                                        QSettings().setValue(QStringLiteral("Agenda/caldavEtags/") + etagKey, resourceEtag);
                                    }
                                }
                            }
                            inCalendarData = false;
                        } else if (xml.name() == QLatin1String("getetag")) {
                            inEtag = false;
                        }
                    }
                }
                parsed = parsed || !xml.hasError();
                if (xml.hasError()) qWarning() << "Invalid CalDAV response:" << xml.errorString();
                if (collectionId >= 0 && privilegeSetSeen) setServerCollectionReadOnly(collectionId, !serverWritable);

                if (parsed) {
                    const auto previousUids = remoteState(urlString);
                    auto deletedUids = deletedRemoteState(urlString);
                    QStringList remoteDeletions;
                    bool changed = false;
                    for (const auto &incidence : m_calendar->incidences()) {
                        const auto uid = incidence->uid();
                        if (deletedUids.contains(uid) && remoteUids.contains(uid) && (collectionId < 0 || incidence->customProperty("MauiCalendar", "CollectionId").toLongLong() == collectionId) && !isCollectionReadOnly(incidence->customProperty("MauiCalendar", "CollectionId").toLongLong())) {
                            m_calendar->deleteIncidence(incidence);
                            if (!remoteDeletions.contains(uid)) remoteDeletions.append(uid);
                            changed = true;
                        } else if (previousUids.contains(uid) && !remoteUids.contains(uid) && (collectionId < 0 || incidence->customProperty("MauiCalendar", "CollectionId").toLongLong() == collectionId) && !isCollectionReadOnly(incidence->customProperty("MauiCalendar", "CollectionId").toLongLong())) {
                            m_calendar->deleteIncidence(incidence);
                            changed = true;
                        }
                    }
                    for (const auto &uid : deletedUids.values()) {
                        if (!remoteUids.contains(uid)) deletedUids.remove(uid);
                    }
                    saveDeletedRemoteState(urlString, deletedUids);
                    if (changed) {
                        save();
                        Q_EMIT calendarChanged();
                    }
                    saveRemoteState(urlString, remoteUids);
                    m_remoteUids.insert(urlString, remoteUids);
                    deleteCalDavResources(urlString, remoteDeletions, username, password);
                }
            }
        }
        reply->deleteLater();
        network->deleteLater();
        if (success && parsed) uploadCalDav(urlString, username, password, collectionId);
    });
}

bool CalendarManager::exportFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }

    const auto exportedCalendar = QSharedPointer<KCalendarCore::MemoryCalendar>::create(QTimeZone::systemTimeZone());
    for (const auto &incidence : m_calendar->incidences()) {
        exportedCalendar->addIncidence(incidenceForSerialization(incidence));
    }
    KCalendarCore::FileStorage exportedStorage(exportedCalendar, filePath);
    if (!exportedStorage.save()) {
        qWarning() << "Failed to export calendar file:" << filePath;
        return false;
    }
    return true;
}

void CalendarManager::addIncidence(IncidenceWrapper *incidenceWrapper)
{
    if (!incidenceWrapper || !incidenceWrapper->incidencePtr()) {
        return;
    }
    if (incidenceWrapper->summary().trimmed().isEmpty() || incidenceWrapper->incidenceEnd() <= incidenceWrapper->incidenceStart()) {
        qWarning() << "Cannot add an event without a title or with an invalid duration";
        return;
    }
    if (isCollectionReadOnly(incidenceWrapper->collectionId())) {
        qWarning() << "Cannot add an event to a read-only collection";
        return;
    }
    KCalendarCore::Incidence::Ptr incidence(incidenceWrapper->incidencePtr()->clone());
    m_calendar->addIncidence(incidence);
    save();
    Q_EMIT calendarChanged();
}

void CalendarManager::editIncidence(IncidenceWrapper *incidenceWrapper, const QDateTime &occurrenceStart)
{
    if (!incidenceWrapper || !incidenceWrapper->incidencePtr()) {
        return;
    }
    if (incidenceWrapper->summary().trimmed().isEmpty() || incidenceWrapper->incidenceEnd() <= incidenceWrapper->incidenceStart()) {
        qWarning() << "Cannot edit an event without a title or with an invalid duration";
        return;
    }
    if (isCollectionReadOnly(incidenceWrapper->collectionId())) {
        qWarning() << "Cannot edit an event in a read-only collection";
        return;
    }

    const auto wrapped = incidenceWrapper->incidencePtr();
    KCalendarCore::Incidence::Ptr base;
    for (const auto &candidate : m_calendar->incidences()) {
        if (candidate->uid() == wrapped->uid() && candidate->customProperty("MauiCalendar", "CollectionId") == wrapped->customProperty("MauiCalendar", "CollectionId") && !candidate->hasRecurrenceId()) {
            base = candidate;
            break;
        }
    }
    if (!base) {
        qWarning() << "Cannot edit: incidence not found:" << wrapped->uid();
        return;
    }

    QDateTime recurrenceId = wrapped->hasRecurrenceId() ? wrapped->recurrenceId() : occurrenceStart;
    const bool editingOccurrence = base->recurs() && recurrenceId.isValid() && recurrenceId != base->dtStart();
    if (editingOccurrence) {
        auto updated = KCalendarCore::Incidence::Ptr(wrapped->clone());
        updated->setRecurrenceId(recurrenceId);
        updated->recurrence()->clear();
        KCalendarCore::Incidence::Ptr existingException;
        for (const auto &candidate : m_calendar->incidences()) {
            if (candidate->uid() == wrapped->uid() && candidate->hasRecurrenceId() && candidate->recurrenceId() == recurrenceId && candidate->customProperty("MauiCalendar", "CollectionId") == wrapped->customProperty("MauiCalendar", "CollectionId")) {
                existingException = candidate;
                break;
            }
        }
        if (existingException) m_calendar->deleteIncidence(existingException);
        m_calendar->addIncidence(updated);
    } else {
        m_calendar->deleteIncidence(base);
        auto updated = KCalendarCore::Incidence::Ptr(wrapped->clone());
        m_calendar->addIncidence(updated);
    }
    save();
    Q_EMIT calendarChanged();
}

void CalendarManager::deleteIncidenceOccurrence(KCalendarCore::Incidence::Ptr incidence, const QDateTime &occurrenceStart)
{
    if (!incidence || !occurrenceStart.isValid()) return;
    KCalendarCore::Incidence::Ptr base;
    for (const auto &candidate : m_calendar->incidences()) {
        if (candidate->uid() == incidence->uid() && candidate->customProperty("MauiCalendar", "CollectionId") == incidence->customProperty("MauiCalendar", "CollectionId") && !candidate->hasRecurrenceId()) {
            base = candidate;
            break;
        }
    }
    if (!base || !base->recurs() || isCollectionReadOnly(base->customProperty("MauiCalendar", "CollectionId").toLongLong())) return;

    const auto recurrenceId = incidence->hasRecurrenceId() ? incidence->recurrenceId() : occurrenceStart;
    const auto existingException = m_calendar->incidence(incidence->uid(), recurrenceId);
    if (existingException) {
        m_calendar->deleteIncidence(existingException);
    } else if (base->allDay()) {
        base->recurrence()->addExDate(occurrenceStart.date());
    } else {
        base->recurrence()->addExDateTime(occurrenceStart);
    }
    save();
    Q_EMIT calendarChanged();
}

void CalendarManager::deleteIncidence(KCalendarCore::Incidence::Ptr incidence, bool deleteChildren)
{
    if (!incidence) {
        return;
    }
    if (isCollectionReadOnly(incidence->customProperty("MauiCalendar", "CollectionId").toLongLong())) {
        qWarning() << "Cannot delete an event from a read-only collection";
        return;
    }
    if (deleteChildren) {
        const auto children = childIncidences(incidence->uid());
        for (const auto &child : children) {
            deleteIncidence(child, true);
        }
    }
    markRemoteDeletion(incidence->uid(), incidence->customProperty("MauiCalendar", "CollectionId").toLongLong());
    m_calendar->deleteIncidence(incidence);
    save();
    Q_EMIT calendarChanged();
}

void CalendarManager::deleteIncidenceSeries(KCalendarCore::Incidence::Ptr incidence)
{
    if (!incidence) return;
    const auto base = m_calendar->incidence(incidence->uid());
    if (!base || isCollectionReadOnly(base->customProperty("MauiCalendar", "CollectionId").toLongLong())) return;
    markRemoteDeletion(base->uid(), base->customProperty("MauiCalendar", "CollectionId").toLongLong());
    const auto series = m_calendar->incidences();
    for (const auto &item : series) {
        if (item->uid() == base->uid()) m_calendar->deleteIncidence(item);
    }
    save();
    Q_EMIT calendarChanged();
}

bool CalendarManager::hasChildren(KCalendarCore::Incidence::Ptr incidence)
{
    return !childIncidences(incidence->uid()).isEmpty();
}

QVariant CalendarManager::getIncidenceSubclassed(KCalendarCore::Incidence::Ptr incidencePtr)
{
    switch (incidencePtr->type()) {
    case KCalendarCore::IncidenceBase::TypeEvent:
        return QVariant::fromValue(m_calendar->event(incidencePtr->instanceIdentifier()));
    case KCalendarCore::IncidenceBase::TypeTodo:
        return QVariant::fromValue(m_calendar->todo(incidencePtr->instanceIdentifier()));
    case KCalendarCore::IncidenceBase::TypeJournal:
        return QVariant::fromValue(m_calendar->journal(incidencePtr->instanceIdentifier()));
    default:
        return QVariant::fromValue(incidencePtr);
    }
}
