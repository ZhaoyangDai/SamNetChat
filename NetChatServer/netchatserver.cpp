/**
 * @file netchatserver.cpp
 * @brief Implementation of NetChatServer class for TCP-based chat server.
 *
 * This source file implements the NetChatServer class, providing asynchronous
 * handling of client connections, user authentication, and message broadcasting.
 * It utilizes Qt's QThreadPool for concurrent request processing and a separate
 * thread for heartbeat monitoring.
 *
 * @author ZhaoyangDai
 * @date 2026-05-15
 * @version 1.0
 */

#include "netchatserver.h"
#include "protocol.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRunnable>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QMutexLocker>

static QString databaseConnectionName()
{
    return QStringLiteral("NetChatServer_%1").arg(quintptr(QThread::currentThreadId()));
}

static QSqlDatabase databaseConnection(const QString &dbPath)
{
    const QString connectionName = databaseConnectionName();
    if (QSqlDatabase::contains(connectionName))
    {
        return QSqlDatabase::database(connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open())
    {
        qDebug() << "Failed to open database:" << db.lastError().text();
    }
    return db;
}

/**
 * @class MessageTask
 * @brief A QRunnable task for processing client messages asynchronously.
 *
 * This class encapsulates a client request and runs it in a worker thread
 * from the thread pool, allowing the main thread to remain responsive.
 */
class MessageTask : public QRunnable
{
public:
    /**
     * @brief Constructs a MessageTask.
     *
     * @param server Pointer to the NetChatServer instance.
     * @param client The client socket.
     * @param msgType The message type.
     * @param payload The message payload.
     */
    MessageTask(NetChatServer *server, QTcpSocket *client, quint16 msgType, const QByteArray &payload)
        : m_server(server), m_client(client), m_msgType(msgType), m_payload(payload)
    {
        setAutoDelete(true); // Automatically delete the task after execution
    }

    /**
     * @brief Runs the task in a worker thread.
     *
     * Processes the client request by calling the server's processRequest method.
     */
    void run() override
    {
        if (!m_server || m_client.isNull())
            return;
        m_server->processRequest(m_client, m_msgType, m_payload);
    }

private:
    NetChatServer *m_server;       ///< Pointer to the server instance.
    QPointer<QTcpSocket> m_client; ///< Pointer to the client socket (safe for deletion).
    quint16 m_msgType;             ///< The message type.
    QByteArray m_payload;          ///< The message payload.
};

NetChatServer::NetChatServer(QObject *parent)
    : m_chatServer(new QTcpServer(this)), m_mutex(), m_threadPool(), m_heartbeatThread(new QThread(this)), m_heartbeatWorker(new HeartbeatWorker())
{
    m_dbPath = QCoreApplication::applicationDirPath() + "/users.db";
    if (!initializeDatabase())
    {
        qDebug() << "Failed to initialize user database.";
    }

    // Configure thread pool for asynchronous request processing
    // Set maximum thread count to the ideal number for the system
    m_threadPool.setMaxThreadCount(QThread::idealThreadCount());
    // Set expiry timeout to 30 seconds for thread reuse
    m_threadPool.setExpiryTimeout(30000);

    connect(m_chatServer, &QTcpServer::newConnection, this, &NetChatServer::onNewConnected);

    // Move heartbeat worker to its own thread for background monitoring
    m_heartbeatWorker->moveToThread(m_heartbeatThread);

    connect(m_heartbeatThread, &QThread::started, m_heartbeatWorker, &HeartbeatWorker::startHeartbeat);
    connect(m_heartbeatThread, &QThread::finished, m_heartbeatWorker, &QObject::deleteLater);
    connect(m_heartbeatWorker, &HeartbeatWorker::heartbeatTriggered, this, &NetChatServer::onHeartbeatTriggered);

    m_heartbeatThread->start();

    openServer();
}

NetChatServer::~NetChatServer()
{
    closeServer();
}

QString NetChatServer::getServerAddress() const
{
    if (!m_chatServer->isListening())
        return "";

    return m_chatServer->serverAddress().toString();
}

QString NetChatServer::getServerPort() const
{
    if (!m_chatServer->isListening())
        return "";
    return QString::number(m_chatServer->serverPort());
}

QString NetChatServer::getClientAddress(const QString &account) const
{
    if (account.isEmpty())
        return "";

    QTcpSocket *client = getClientByAccount(account);
    if (!client)
        return "";
    return client->localAddress().toString();
}

QVector<QPair<quint32, QString>> NetChatServer::getAllUserRecords() const
{
    QVector<QPair<quint32, QString>> records;
    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
        return records;

    QSqlQuery query(db);
    query.prepare("SELECT userId, username FROM users ORDER BY userId");
    if (!query.exec())
        return records;

    while (query.next())
    {
        records.append({query.value(0).toUInt(), query.value(1).toString()});
    }

    return records;
}

QTcpSocket *NetChatServer::getClientByAccount(const QString &account) const
{
    if (account.isEmpty())
    {
        return nullptr;
    }

    // Try numeric id first
    bool ok = false;
    quint32 id = account.toUInt(&ok);
    if (ok)
    {
        if (m_userClientMap.contains(id))
        {
            QTcpSocket *client = m_userClientMap.value(id);
            if (client && client->state() == QAbstractSocket::ConnectedState)
                return client;
        }
    }

    // Otherwise try to resolve by userName
    quint32 resolvedId = getUserIdByUserName(account);
    if (resolvedId != 0 && m_userClientMap.contains(resolvedId))
    {
        QTcpSocket *client = m_userClientMap.value(resolvedId);
        if (client && client->state() == QAbstractSocket::ConnectedState)
            return client;
    }

    return nullptr;
}

QTcpSocket *NetChatServer::getChatClient(const QString &account) const
{
    return getClientByAccount(account);
}

void NetChatServer::sendChatMessage(const QString &msg)
{
    sendChatMessage("", msg);
}

void NetChatServer::sendChatMessage(const QString &sender, const QString &message)
{
    // Reuse broadcastMessage to send user or server chat to every logged-in client.
    broadcastMessage(sender, message, nullptr);
}

void NetChatServer::processRequest(QTcpSocket *client, quint16 msgType, const QByteArray &payload)
{
    if (!client)
        return;

    switch (msgType)
    {
    case MsgType::RegisterReq:
        processRegisterRequest(client, payload);
        break;
    case MsgType::LoginReq:
        processLoginRequest(client, payload);
        break;
    case MsgType::ChatMessage:
        processChatMessage(client, payload);
        break;
    case MsgType::ForwardMsgReq:
        processForwardMessage(client, payload);
        break;
    case MsgType::AddFriendReq:
        processAddFriendRequest(client, payload);
        break;
    case MsgType::DeleteFriendReq:
        processDeleteFriendRequest(client, payload);
        break;
    default:
        qDebug() << "Unknown message type:" << msgType;
        break;
    }
}

void NetChatServer::processRegisterRequest(QTcpSocket *client, const QByteArray &payload)
{
    qDebug() << Q_FUNC_INFO;
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    QString userName, password;
    in >> userName >> password;
    qDebug() << Q_FUNC_INFO << " user: " << userName << " password: " << password;

    if (userName.isEmpty() || password.isEmpty())
    {
        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::RegisterResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Username and password cannot be empty."),
                                  Q_ARG(quint32, 0),
                                  Q_ARG(QString, QString()));
        return;
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
    {
        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::RegisterResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Registration failed: database unavailable."),
                                  Q_ARG(quint32, 0),
                                  Q_ARG(QString, QString()));
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT userId FROM users WHERE username = :username");
    query.bindValue(":username", userName);
    if (!query.exec())
    {
        qDebug() << Q_FUNC_INFO << "Database error:" << query.lastError().text();
    }

    if (query.next())
    {
        qDebug() << Q_FUNC_INFO << "User Already Exists.";
        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::RegisterResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Username already exists."),
                                  Q_ARG(quint32, 0),
                                  Q_ARG(QString, QString()));
        return;
    }

    quint32 userId = generateUserId();
    if (userId == 0)
    {
        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::RegisterResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Registration failed: could not generate user ID."),
                                  Q_ARG(quint32, 0),
                                  Q_ARG(QString, QString()));
        return;
    }

    query.prepare("INSERT INTO users (userId, username, password, online) VALUES (:userId, :username, :password, 0)");
    query.bindValue(":userId", static_cast<quint32>(userId));
    query.bindValue(":username", userName);
    query.bindValue(":password", password);
    if (!query.exec())
    {
        qDebug() << Q_FUNC_INFO << "Insert failed:" << query.lastError().text();
        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::RegisterResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Registration failed: database error."),
                                  Q_ARG(quint32, 0),
                                  Q_ARG(QString, QString()));
        return;
    }

    QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                              Q_ARG(QTcpSocket *, client),
                              Q_ARG(quint16, MsgType::RegisterResp),
                              Q_ARG(bool, true),
                              Q_ARG(QString, "Registration successful!"),
                              Q_ARG(quint32, userId),
                              Q_ARG(QString, userName));
    emit userListChanged();
    qDebug() << "User registered:" << userName << "userId:" << userId;
}

void NetChatServer::processLoginRequest(QTcpSocket *client, const QByteArray &payload)
{
    qDebug() << Q_FUNC_INFO;
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    QString userAccount, password;
    in >> userAccount >> password;
    qDebug() << Q_FUNC_INFO << " userAccount: " << userAccount << " password: " << password;

    if (userAccount.isEmpty() || password.isEmpty())
    {
        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::LoginResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Account and password cannot be empty."),
                                  Q_ARG(quint32, 0),
                                  Q_ARG(QString, QString()));
        return;
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
    {
        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::LoginResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Login failed: database unavailable."),
                                  Q_ARG(QString, QString()));
        return;
    }

    quint32 userId = 0;
    QString userName;
    QString storedPassword;
    bool userOnline = false;

    {
        QSqlQuery query(db);
        bool ok = false;
        if (userAccount.size() > 0 && userAccount.toUInt(&ok))
        {
            query.prepare("SELECT userId, username, password, online FROM users WHERE userId = :identifier");
            query.bindValue(":identifier", userAccount.toUInt());
            if (query.exec() && query.next())
            {
                userId = query.value(0).toUInt(&ok);
                userName = query.value(1).toString();
                storedPassword = query.value(2).toString();
                userOnline = query.value(3).toInt() == 1;
            }
        }

        if (userId == 0)
        {
            query.prepare("SELECT userId, username, password, online FROM users WHERE username = :identifier");
            query.bindValue(":identifier", userAccount);
            if (!query.exec() || !query.next())
            {
                QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                          Q_ARG(QTcpSocket *, client),
                                          Q_ARG(quint16, MsgType::LoginResp),
                                          Q_ARG(bool, false),
                                          Q_ARG(QString, "Invalid account or password."),
                                          Q_ARG(quint32, 0),
                                          Q_ARG(QString, QString()));
                return;
            }
            userId = query.value(0).toUInt(&ok);
            userName = query.value(1).toString();
            storedPassword = query.value(2).toString();
            userOnline = query.value(3).toInt() == 1;
        }

        if (storedPassword != password)
        {
            QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                      Q_ARG(QTcpSocket *, client),
                                      Q_ARG(quint16, MsgType::LoginResp),
                                      Q_ARG(bool, false),
                                      Q_ARG(QString, "Invalid account or password."),
                                      Q_ARG(quint32, 0),
                                      Q_ARG(QString, QString()));
            return;
        }

        {
            QMutexLocker locker(&m_mutex);
            if (m_loggedInUserMap.contains(client) && m_loggedInUserMap.value(client) != 0)
            {
                QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                          Q_ARG(QTcpSocket *, client),
                                          Q_ARG(quint16, MsgType::LoginResp),
                                          Q_ARG(bool, false),
                                          Q_ARG(QString, "You are already logged in."),
                                          Q_ARG(quint32, 0),
                                          Q_ARG(QString, QString()));
                return;
            }

            if (userOnline && m_userClientMap.contains(userId) && m_userClientMap.value(userId) && m_userClientMap.value(userId)->state() == QAbstractSocket::ConnectedState)
            {
                QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                          Q_ARG(QTcpSocket *, client),
                                          Q_ARG(quint16, MsgType::LoginResp),
                                          Q_ARG(bool, false),
                                          Q_ARG(QString, "This account is already online."),
                                          Q_ARG(quint32, 0),
                                          Q_ARG(QString, QString()));
                return;
            }

            m_loggedInUserMap.insert(client, userId);
            m_userClientMap.insert(userId, client);
        }

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE users SET online = 1 WHERE userId = :userId");
        updateQuery.bindValue(":userId", userId);
        if (!updateQuery.exec())
        {
            qDebug() << Q_FUNC_INFO << "Failed to update online status:" << updateQuery.lastError().text();
        }

        QMetaObject::invokeMethod(this, "sendAuthResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::LoginResp),
                                  Q_ARG(bool, true),
                                  Q_ARG(QString, "Login successful!"),
                                  Q_ARG(quint32, userId),
                                  Q_ARG(QString, userName));

        // Send contact list to client after successful login
        QList<QPair<quint32, QString>> contactList = getFriendsList(userId);
        QByteArray contactListPayload = packContactList(contactList);
        QByteArray contactListMessage = packMessage(MsgType::ContactListResp, contactListPayload);
        client->write(contactListMessage);

        emit userListChanged();
        QMetaObject::invokeMethod(this, "broadcastMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, "Server"),
                                  Q_ARG(QString, userName + " has joined the chat."),
                                  Q_ARG(QTcpSocket *, nullptr));
        qDebug() << userName << "logged in.";
    }
}

void NetChatServer::processChatMessage(QTcpSocket *client, const QByteArray &payload)
{
    qDebug() << Q_FUNC_INFO;
    bool loggedIn = false;
    quint32 userId = 0;
    QString sender;
    {
        // Thread-safe check of login status
        QMutexLocker locker(&m_mutex);
        loggedIn = m_loggedInUserMap.contains(client) && m_loggedInUserMap.value(client) != 0;
        if (loggedIn)
            userId = m_loggedInUserMap.value(client);
    }

    if (!loggedIn)
    {
        qDebug() << Q_FUNC_INFO << "Not login.";
        // Invoke response in main thread
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ChatMessage),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Please login first before chatting."));
        return;
    }

    sender = getUserNameByUserId(userId);
    if (sender.isEmpty())
    {
        sender = QString::number(userId);
    }

    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    QString message;
    in >> message;
    qDebug() << sender << ":" << message;

    // Invoke broadcast in main thread to ensure thread safety for socket operations
    QMetaObject::invokeMethod(this, "broadcastMessage", Qt::QueuedConnection,
                              Q_ARG(QString, sender),
                              Q_ARG(QString, message),
                              Q_ARG(QTcpSocket *, client));
}

// ============ Forward Message Processing ============
// Handles user-to-user private message forwarding through server as relay point.
// Protocol: Client sends [targetUserAccount][message] -> Server forwards [fromUserId][fromUserName][message]
void NetChatServer::processForwardMessage(QTcpSocket *client, const QByteArray &payload)
{
    qDebug() << Q_FUNC_INFO << "Processing forward message request";

    // Verify sender is logged in
    bool loggedIn = false;
    quint32 senderUserId = 0;
    QString senderUserName;
    {
        QMutexLocker locker(&m_mutex);
        loggedIn = m_loggedInUserMap.contains(client) && m_loggedInUserMap.value(client) != 0;
        if (loggedIn)
            senderUserId = m_loggedInUserMap.value(client);
    }

    if (!loggedIn)
    {
        qDebug() << Q_FUNC_INFO << "Sender not logged in";
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Please login first before sending forwarded messages."));
        return;
    }

    // Unpack forward request: either [targetUserAccount][message] or [marker][targetUserId][message]
    QString targetAccount;
    quint32 targetUserId = 0;
    QString message;
    QDataStream payloadIn(payload);
    payloadIn.setVersion(QDataStream::Qt_6_0);
    quint32 marker = 0;
    payloadIn >> marker;
    if (marker == kNumericUserIdRequestMarker)
    {
        payloadIn >> targetUserId >> message;
    }
    else
    {
        unpackForwardMsgRequest(payload, targetAccount, message);
    }

    senderUserName = getUserNameByUserId(senderUserId);
    if (senderUserName.isEmpty())
        senderUserName = QString::number(senderUserId);
    if (targetUserId != 0 && targetAccount.isEmpty())
        targetAccount = QString::number(targetUserId);

    qDebug() << Q_FUNC_INFO << "From:" << senderUserName << "(" << senderUserId << ")"
             << "To:" << targetAccount << "Message:" << message;

    // Validate target userId exists in database
    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Server error: database unavailable"));
        return;
    }

    // Check if target user exists
    // Support both userId and username as target identifiers.
    quint32 resolvedTargetUserId = 0;
    if (targetUserId == 0 && targetAccount.isEmpty())
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Target user cannot be empty"));
        return;
    }

    if (targetUserId != 0)
    {
        resolvedTargetUserId = targetUserId;
    }
    else
    {
        bool ok = false;
        quint32 numeric = targetAccount.toUInt(&ok);
        if (ok)
        {
            resolvedTargetUserId = numeric;
        }
        else
        {
            resolvedTargetUserId = getUserIdByUserName(targetAccount);
        }
    }

    // Prevent self-messaging after target resolution
    if (senderUserId == resolvedTargetUserId)
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Cannot send message to yourself"));
        return;
    }
    if (resolvedTargetUserId == 0)
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Target user cannot be empty"));
        return;
    }
    QSqlQuery query(db);
    query.prepare("SELECT username FROM users WHERE userId = :userId");
    query.bindValue(":userId", resolvedTargetUserId);
    if (!query.exec() || !query.next())
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Target user not found"));
        return;
    }

    // Get target client socket if currently online
    QTcpSocket *targetClient = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        if (m_userClientMap.contains(resolvedTargetUserId))
        {
            targetClient = m_userClientMap.value(resolvedTargetUserId);
            if (!targetClient || targetClient->state() != QAbstractSocket::ConnectedState)
                targetClient = nullptr;
        }
    }

    if (targetClient)
    {
        // Target is online: forward message immediately
        // Format: [ForwardMsgReq][fromUserId][message]
        QByteArray forwardPayload;
        QDataStream out(&forwardPayload, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << QString::number(senderUserId) << senderUserName << message;

        // Forward message to target
        QMetaObject::invokeMethod(this, "sendForwardResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, targetClient),
                                  Q_ARG(quint16, MsgType::ForwardMsgReq),
                                  Q_ARG(bool, true),
                                  Q_ARG(quint32, senderUserId),
                                  Q_ARG(QString, senderUserName),
                                  Q_ARG(QString, message));

        // Send success response to sender
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, true),
                                  Q_ARG(QString, "Message forwarded successfully"));

        qDebug() << Q_FUNC_INFO << "Message delivered to online user:" << targetAccount;
    }
    else
    {
        // Target is offline: respond with offline status
        // In future, could implement offline message storage
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::ForwardMsgResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Target user is currently offline"));

        qDebug() << Q_FUNC_INFO << "Target user offline:" << targetAccount;
    }
}

// ============ Add Friend Processing ============
// Handles friend search and validation requests.
// Protocol: Client sends [userAccount] -> Server responds [success][message][userId][userName]
void NetChatServer::processAddFriendRequest(QTcpSocket *client, const QByteArray &payload)
{
    qDebug() << Q_FUNC_INFO << "Processing add friend request";

    // Verify requester is logged in
    bool loggedIn = false;
    quint32 requesterUserId = 0;
    QString requesterUserName;
    {
        QMutexLocker locker(&m_mutex);
        loggedIn = m_loggedInUserMap.contains(client) && m_loggedInUserMap.value(client) != 0;
        if (loggedIn)
            requesterUserId = m_loggedInUserMap.value(client);
    }

    if (!loggedIn)
    {
        qDebug() << Q_FUNC_INFO << "Requester not logged in";
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::AddFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Please login first before adding friends."));
        return;
    }

    // Unpack request: either [userAccount] or [marker][userId]
    QString userAccount;
    quint32 targetUserId = 0;
    QDataStream requestIn(payload);
    requestIn.setVersion(QDataStream::Qt_6_0);
    quint32 marker = 0;
    requestIn >> marker;
    if (marker == kNumericUserIdRequestMarker)
    {
        requestIn >> targetUserId;
    }
    else
    {
        unpackAddFriendRequest(payload, userAccount);
    }

    requesterUserName = getUserNameByUserId(requesterUserId);
    if (requesterUserName.isEmpty())
        requesterUserName = QString::number(requesterUserId);
    if (targetUserId != 0 && userAccount.isEmpty())
        userAccount = QString::number(targetUserId);

    qDebug() << Q_FUNC_INFO << "Requester:" << requesterUserName << "(" << requesterUserId << ")"
             << "Target:" << userAccount;

    // Validate database is available
    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::AddFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Server error: database unavailable"));
        return;
    }

    // Validate input not empty
    if (targetUserId == 0 && userAccount.isEmpty())
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::AddFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Username or ID cannot be empty"));
        return;
    }

    // Resolve target user: try numeric userId first, then username
    quint32 resolvedTargetUserId = 0;
    QString resolvedTargetUsername;

    if (targetUserId != 0)
    {
        QSqlQuery query(db);
        query.prepare("SELECT username FROM users WHERE userId = :userId");
        query.bindValue(":userId", targetUserId);
        if (query.exec() && query.next())
        {
            resolvedTargetUserId = targetUserId;
            resolvedTargetUsername = query.value(0).toString();
        }
    }
    else
    {
        bool ok = false;
        quint32 numeric = userAccount.toUInt(&ok);
        if (ok)
        {
            QSqlQuery query(db);
            query.prepare("SELECT username FROM users WHERE userId = :userId");
            query.bindValue(":userId", numeric);
            if (query.exec() && query.next())
            {
                resolvedTargetUserId = numeric;
                resolvedTargetUsername = query.value(0).toString();
            }
        }
    }

    // If not found by id, try by username
    if (resolvedTargetUserId == 0 && !userAccount.isEmpty())
    {
        QSqlQuery query(db);
        query.prepare("SELECT userId FROM users WHERE username = :username");
        query.bindValue(":username", userAccount);
        if (query.exec() && query.next())
        {
            resolvedTargetUserId = query.value(0).toUInt();
            resolvedTargetUsername = userAccount;
        }
    }

    // Check if target user exists
    if ((0 == resolvedTargetUserId) || resolvedTargetUsername.isEmpty())
    {
        qDebug() << Q_FUNC_INFO << "Target user not found:" << userAccount;
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::AddFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "User does not exist"));
        return;
    }

    // Prevent adding self as friend
    if (requesterUserId == resolvedTargetUserId)
    {
        qDebug() << Q_FUNC_INFO << "User attempted to add self as friend";
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::AddFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Cannot add yourself as a friend"));
        return;
    }

    // Success: send friend info back to requester
    qDebug() << Q_FUNC_INFO << "Friend found:" << resolvedTargetUsername << "(" << resolvedTargetUserId << ")";

    // Save friend relationship to database
    if (addFriend(requesterUserId, resolvedTargetUserId))
    {
        qDebug() << Q_FUNC_INFO << "Friend relationship saved to database";
    }

    QMetaObject::invokeMethod(this, "sendAddFriendResponse", Qt::QueuedConnection,
                              Q_ARG(QTcpSocket *, client),
                              Q_ARG(quint16, MsgType::AddFriendResp),
                              Q_ARG(bool, true),
                              Q_ARG(QString, "Friend added successfully"),
                              Q_ARG(quint32, resolvedTargetUserId),
                              Q_ARG(QString, resolvedTargetUsername));

    emit clientReadyRead("Server", "Friend added: " + resolvedTargetUsername);
}

void NetChatServer::processDeleteFriendRequest(QTcpSocket *client, const QByteArray &payload)
{
    qDebug() << Q_FUNC_INFO;

    // Check if user is logged in
    bool loggedIn = false;
    quint32 userId = 0;
    {
        QMutexLocker locker(&m_mutex);
        loggedIn = m_loggedInUserMap.contains(client) && m_loggedInUserMap.value(client) != 0;
        if (loggedIn)
            userId = m_loggedInUserMap.value(client);
    }

    if (!loggedIn)
    {
        qDebug() << Q_FUNC_INFO << "User not logged in";
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::DeleteFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Not logged in"));
        return;
    }

    quint32 friendId = 0;
    unpackDeleteFriendRequest(payload, friendId);
    qDebug() << Q_FUNC_INFO << "Delete friend request from userId:" << userId << "for friendId:" << friendId;

    if (friendId == 0 || friendId == userId)
    {
        qDebug() << Q_FUNC_INFO << "Invalid friendId:" << friendId;
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::DeleteFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Invalid friend ID"));
        return;
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
    {
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::DeleteFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Database error"));
        return;
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM friends WHERE userId = :userId AND friendId = :friendId");
    deleteQuery.bindValue(":userId", userId);
    deleteQuery.bindValue(":friendId", friendId);
    if (!deleteQuery.exec())
    {
        qDebug() << Q_FUNC_INFO << "Delete failed:" << deleteQuery.lastError().text();
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::DeleteFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Failed to delete friend"));
        return;
    }

    if (deleteQuery.numRowsAffected() == 0)
    {
        qDebug() << Q_FUNC_INFO << "Friend relationship not found";
        QMetaObject::invokeMethod(this, "sendResponse", Qt::QueuedConnection,
                                  Q_ARG(QTcpSocket *, client),
                                  Q_ARG(quint16, MsgType::DeleteFriendResp),
                                  Q_ARG(bool, false),
                                  Q_ARG(QString, "Friend not found"));
        return;
    }

    qDebug() << Q_FUNC_INFO << "Friend deleted successfully";
    QMetaObject::invokeMethod(this, "sendDelFriendResponse", Qt::QueuedConnection,
                              Q_ARG(QTcpSocket *, client),
                              Q_ARG(quint16, MsgType::DeleteFriendResp),
                              Q_ARG(bool, true),
                              Q_ARG(QString, "Friend deleted successfully"),
                              Q_ARG(quint32, friendId),
                              Q_ARG(QString, "Ignore"));

    emit clientReadyRead("Server", "Friend deleted");
}

void NetChatServer::openServer()
{
    // m_tcpServer->listen();

    // QString ipAddress;
    // const QList<QHostAddress> ipAddressList = QNetworkInterface::allAddresses();
    // for (const QHostAddress &hostAddress : ipAddressList) {
    //     if (hostAddress != QHostAddress::LocalHost && hostAddress.toIPv4Address()) {
    //         ipAddress = hostAddress.toString();
    //         break;
    //     }
    // }
    // if (ipAddress.isEmpty()) {
    //     ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
    // }

    m_chatServer->listen(QHostAddress::LocalHost, 6688);
}

void NetChatServer::closeServer()
{
    // Wait for all pending tasks in the thread pool to complete
    // This ensures no requests are lost during shutdown
    m_threadPool.waitForDone();

    // Close all client connections gracefully
    for (auto it = m_loggedInUserMap.begin(); it != m_loggedInUserMap.end(); ++it)
    {
        QTcpSocket *client = it.key();
        if (client && client->state() == QAbstractSocket::ConnectedState)
        {
            client->disconnectFromHost();
        }
    }

    // Clear all maps
    m_loggedInUserMap.clear();
    m_expectedSizeMap.clear();

    // Close the TCP server
    m_chatServer->close();

    // Stop the heartbeat thread
    if (m_heartbeatThread && m_heartbeatThread->isRunning())
    {
        m_heartbeatThread->quit();
        m_heartbeatThread->wait(); // Wait for the thread to finish
    }
}

bool NetChatServer::initializeDatabase()
{
    QDir dir(QCoreApplication::applicationDirPath());
    if (!dir.exists())
    {
        dir.mkpath(dir.absolutePath());
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
    {
        return false;
    }

    if (!createUserTable())
    {
        return false;
    }

    if (!createFriendsTable())
    {
        return false;
    }

    return true;
}

bool NetChatServer::createUserTable()
{
    QSqlDatabase db = databaseConnection(m_dbPath);
    QSqlQuery query(db);
    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS users ("
        "userId INTEGER PRIMARY KEY, "
        "username TEXT UNIQUE NOT NULL, "
        "password TEXT NOT NULL, "
        "online INTEGER NOT NULL DEFAULT 0"
        ")");
    return query.exec(ddl);
}

bool NetChatServer::createFriendsTable()
{
    QSqlDatabase db = databaseConnection(m_dbPath);
    QSqlQuery query(db);
    const QString ddl = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS friends ("
        "userId INTEGER NOT NULL, "
        "friendId INTEGER NOT NULL, "
        "PRIMARY KEY (userId, friendId), "
        "FOREIGN KEY (userId) REFERENCES users(userId), "
        "FOREIGN KEY (friendId) REFERENCES users(userId)"
        ")");
    return query.exec(ddl);
}

quint32 NetChatServer::generateUserId() const
{
    // Generate a random non-zero 32-bit user id and ensure uniqueness in DB
    QSqlDatabase db = databaseConnection(m_dbPath);
    for (int retry = 0; retry < 10; ++retry)
    {
        quint32 id = QRandomGenerator::global()->generate();
        if (id == 0)
            continue;
        QSqlQuery query(db);
        query.prepare("SELECT 1 FROM users WHERE userId = :userId");
        query.bindValue(":userId", id);
        if (query.exec() && !query.next())
        {
            return id;
        }
    }
    return 0;
}

quint32 NetChatServer::getUserIdByUserName(const QString &userName) const
{
    if (userName.isEmpty())
    {
        return 0;
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    QSqlQuery query(db);
    query.prepare("SELECT userId FROM users WHERE username = :username");
    query.bindValue(":username", userName);
    if (query.exec() && query.next())
    {
        return query.value(0).toUInt();
    }
    return 0;
}

QString NetChatServer::getUserNameByUserId(quint32 userId) const
{
    if (userId == 0)
    {
        return QString();
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    QSqlQuery query(db);
    query.prepare("SELECT username FROM users WHERE userId = :userId");
    query.bindValue(":userId", userId);
    if (query.exec() && query.next())
    {
        return query.value(0).toString();
    }
    return QString();
}

bool NetChatServer::addFriend(quint32 userId, quint32 friendId)
{
    if (userId == 0 || friendId == 0 || userId == friendId)
    {
        return false;
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO friends (userId, friendId) VALUES (:userId, :friendId)");
    query.bindValue(":userId", userId);
    query.bindValue(":friendId", friendId);
    return query.exec();
}

QList<QPair<quint32, QString>> NetChatServer::getFriendsList(quint32 userId) const
{
    QList<QPair<quint32, QString>> friends;
    if (userId == 0)
    {
        return friends;
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    QSqlQuery query(db);
    query.prepare("SELECT f.friendId, u.username FROM friends f "
                  "JOIN users u ON f.friendId = u.userId "
                  "WHERE f.userId = :userId");
    query.bindValue(":userId", userId);
    if (query.exec())
    {
        while (query.next())
        {
            quint32 friendId = query.value(0).toUInt();
            QString friendName = query.value(1).toString();
            friends.append(qMakePair(friendId, friendName));
        }
    }
    return friends;
}

QTcpSocket *NetChatServer::getClientByUserName(const QString &userName) const
{
    return getClientByAccount(userName);
}

QTcpSocket *NetChatServer::getClientByUserId(quint32 userId) const
{
    // QMutexLocker locker(&m_mutex);
    if (userId == 0)
        return nullptr;
    return getClientByAccount(QString::number(userId));
}

bool NetChatServer::deleteOfflineUser(quint32 userId)
{
    if (userId == 0)
        return false;

    QMutexLocker locker(&m_mutex);
    if (m_userClientMap.contains(userId) && m_userClientMap.value(userId) && m_userClientMap.value(userId)->state() == QAbstractSocket::ConnectedState)
    {
        return false;
    }

    QSqlDatabase db = databaseConnection(m_dbPath);
    if (!db.isOpen())
        return false;

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT online FROM users WHERE userId = :userId");
    checkQuery.bindValue(":userId", userId);
    if (!checkQuery.exec() || !checkQuery.next())
        return false;

    if (checkQuery.value(0).toInt() != 0)
        return false;

    QSqlQuery cleanupQuery(db);
    cleanupQuery.prepare("DELETE FROM friends WHERE userId = :userId OR friendId = :userId");
    cleanupQuery.bindValue(":userId", userId);
    if (!cleanupQuery.exec())
        return false;

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM users WHERE userId = :userId AND online = 0");
    deleteQuery.bindValue(":userId", userId);
    if (!deleteQuery.exec())
        return false;

    if (deleteQuery.numRowsAffected() == 0)
        return false;

    emit userListChanged();
    return true;
}

void NetChatServer::sendResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message;
    client->write(packMessage(msgType, payload));
    emit clientReadyRead("Server", message);
}

void NetChatServer::sendAuthResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message, quint32 userId, const QString &userName)
{
    QByteArray payload = packAuthResponse(success, message, userId, userName);
    client->write(packMessage(msgType, payload));
    emit clientReadyRead("Server", message);
}

void NetChatServer::sendForwardResponse(QTcpSocket *client, quint16 msgType, bool success, quint32 senderId, const QString &senderName, const QString &senderMessage)
{
    QByteArray payload = packForwardedMessage(senderId, senderName, senderMessage);
    client->write(packMessage(msgType, payload));
    emit clientReadyRead("Server", senderMessage);
}

void NetChatServer::sendAddFriendResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message, quint32 userId, const QString &userName)
{
    QByteArray payload = packAddFriendResponse(true, message, userId, userName);
    client->write(packMessage(msgType, payload));
    emit clientReadyRead("Server", message);
}

void NetChatServer::sendDelFriendResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message, quint32 userId, const QString &userName)
{
    QByteArray payload = packDeleteFriendResponse(true, message, userId, userName);
    client->write(packMessage(msgType, payload));
    emit clientReadyRead("Server", message);
}

void NetChatServer::broadcastMessage(const QString &sender, const QString &message, QTcpSocket *excludeSocket)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    if (sender.isEmpty())
        out << message;
    else
        out << sender << message;

    for (QTcpSocket *client : m_loggedInUserMap.keys())
    {
        if (client != excludeSocket && client->state() == QAbstractSocket::ConnectedState)
        {
            client->write(packMessage(MsgType::ChatMessage, payload));
        }
    }
    emit clientReadyRead("Broadcast", message);
}

void NetChatServer::onNewConnected()
{
    QString welcomeMsg = "Server connected, Welcome!";
    qDebug() << Q_FUNC_INFO << welcomeMsg;

    QTcpSocket *client = m_chatServer->nextPendingConnection();
    if (client == nullptr)
    {
        qDebug() << Q_FUNC_INFO << "error: No connect.";
        return;
    }

    m_expectedSizeMap.insert(client, 0);

    connect(client, &QTcpSocket::readyRead, this, &NetChatServer::onReadyRead);
    connect(client, &QAbstractSocket::disconnected, this, &NetChatServer::onDisConnected);

    emit clientConnected();
}

void NetChatServer::onReadyRead()
{
    qDebug() << Q_FUNC_INFO;
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client)
    {
        return;
    }

    QDataStream in(client);
    in.setVersion(QDataStream::Qt_6_0);

    quint32 size = m_expectedSizeMap[client];

    while (true)
    {
        if (size == 0)
        {
            if (client->bytesAvailable() < sizeof(quint32))
                break;
            in >> size;
            m_expectedSizeMap[client] = size;
        }

        if (client->bytesAvailable() < size)
            break;

        quint16 msgType;
        QByteArray payload;
        in >> msgType >> payload;
        size = 0;
        m_expectedSizeMap[client] = 0;

        qDebug() << Q_FUNC_INFO << "Message type: " << msgType;
        // Submit the request to the thread pool for asynchronous processing
        // This keeps the main thread responsive and allows concurrent handling of multiple clients
        m_threadPool.start(new MessageTask(this, client, msgType, payload));
    }
}

void NetChatServer::onDisConnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (!client)
    {
        return;
    }
    quint32 userId = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (m_loggedInUserMap.contains(client))
        {
            userId = m_loggedInUserMap.value(client);
            m_loggedInUserMap.remove(client);
        }
        m_expectedSizeMap.remove(client);

        if (userId != 0)
        {
            m_userClientMap.remove(userId);
        }
    }

    if (userId != 0)
    {
        QSqlDatabase db = databaseConnection(m_dbPath);
        if (db.isOpen())
        {
            QSqlQuery query(db);
            query.prepare("UPDATE users SET online = 0 WHERE userId = :userId");
            query.bindValue(":userId", userId);
            if (!query.exec())
            {
                qDebug() << Q_FUNC_INFO << "Failed to update offline status:" << query.lastError().text();
            }
        }
        emit userListChanged();
    }
}

void NetChatServer::onHeartbeatTriggered()
{
    broadcastMessage(QString(), "Heartbeat detection - Server connection is normal.");
}
