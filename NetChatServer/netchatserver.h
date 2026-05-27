/**
 * @file netchatserver.h
 * @brief NetChatServer class definition for handling TCP-based chat server functionality.
 *
 * This header file defines the NetChatServer class, which manages client connections,
 * user authentication, and message broadcasting using Qt's networking and threading capabilities.
 *
 * @author ZhaoyangDai
 * @date 2026-05-15
 * @version 1.0
 */

#ifndef NETCHATSERVER_H
#define NETCHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QThreadPool>
#include <QMutex>
#include <QHash>
#include <QPointer>
#include <QVector>
#include <QPair>
#include <QString>

#include "heartbeatworker.h"

/**
 * @class NetChatServer
 * @brief A TCP-based chat server that handles multiple client connections asynchronously.
 *
 * The NetChatServer class provides functionality for user registration, login, and real-time
 * chat messaging. It uses a thread pool for handling client requests concurrently and
 * maintains a separate thread for heartbeat monitoring.
 *
 * Key features:
 * - Asynchronous request processing using QThreadPool
 * - User authentication and session management
 * - Message broadcasting to all connected clients
 * - Heartbeat mechanism for connection monitoring
 *
 * @note This class is designed for Qt 6.x and uses Qt's networking and threading modules.
 */
class NetChatServer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a NetChatServer instance.
     *
     * Initializes the TCP server, thread pool, and heartbeat worker.
     * Starts the server listening on localhost:6688.
     *
     * @param parent The parent QObject, defaults to nullptr.
     *
     * @example
     * NetChatServer *server = new NetChatServer(this);
     */
    explicit NetChatServer(QObject *parent = nullptr);

    /**
     * @brief Destroys the NetChatServer instance.
     *
     * Closes all client connections, stops the thread pool, and cleans up resources.
     */
    ~NetChatServer();

    /**
     * @brief Gets the server address as a string.
     *
     * @return The server IP address if listening, otherwise an empty string.
     */
    QString getServerAddress() const;

    /**
     * @brief Gets the server port as a string.
     *
     * @return The server port number if listening, otherwise an empty string.
     */
    QString getServerPort() const;

    /**
     * @brief Gets the client address for a specific user.
     *
     * @param account The username or userId to look up.
     * @return The client's IP address if found, otherwise an empty string.
     */
    QString getClientAddress(const QString &account = QString()) const;

    /**
     * @brief Gets the user data map.
     *
     * @return A pointer to the map containing cached userId-username pairs.
     */
    QVector<QPair<quint32, QString>> getAllUserRecords() const;

    /**
     * @brief Gets a bound client socket by username or userId.
     *
     * @param account The username or userId.
     * @return The bound QTcpSocket if available, otherwise nullptr.
     */
    QTcpSocket *getClientByAccount(const QString &account) const;

    /**
     * @brief Sends a chat message from the server.
     *
     * @param msg The message to send.
     */
    void sendChatMessage(const QString &msg);

    /**
     * @brief Sends a chat message with a specified sender.
     *
     * @param sender The sender's name.
     * @param message The message content.
     */
    void sendChatMessage(const QString &sender, const QString &message);
    bool deleteOfflineUser(quint32 userId);

    /**
     * @brief Processes a client request asynchronously.
     *
     * This method is called from worker threads to handle different message types.
     *
     * @param client The client socket.
     * @param msgType The message type.
     * @param payload The message payload.
     */
    void processRequest(QTcpSocket *client, quint16 msgType, const QByteArray &payload);

    /**
     * @brief Processes a user registration request.
     *
     * @param client The client socket.
     * @param payload The registration payload containing userName and password.
     */
    void processRegisterRequest(QTcpSocket *client, const QByteArray &payload);

    /**
     * @brief Processes a user login request.
     *
     * @param client The client socket.
     * @param payload The login payload containing userId/userName and password.
     */
    void processLoginRequest(QTcpSocket *client, const QByteArray &payload);

    /**
     * @brief Processes a chat message request.
     *
     * @param client The client socket.
     * @param payload The chat payload containing the message.
     */
    void processChatMessage(QTcpSocket *client, const QByteArray &payload);

    /**
     * @brief Processes a forward message request.
     *
     * Routes the message from sender to the specified target user.
     * If target is offline, stores the message for later delivery.
     *
     * @param client The client socket (sender).
     * @param payload The forward request payload containing target user account and message.
     */
    void processForwardMessage(QTcpSocket *client, const QByteArray &payload);

    /**
     * @brief Processes an add friend request.
     *
     * Validates if the target user exists and returns the friend info.
     *
     * @param client The client socket (requester).
     * @param payload The add friend request payload containing user account.
     */
    void processAddFriendRequest(QTcpSocket *client, const QByteArray &payload);

    /**
     * @brief Processes a delete friend request.
     *
     * Removes the specified friend from the user's friend list.
     *
     * @param client The client socket (requester).
     * @param payload The delete friend request payload containing friendId.
     */
    void processDeleteFriendRequest(QTcpSocket *client, const QByteArray &payload);

private:
    void openServer();
    void closeServer();

    bool initializeDatabase();
    bool createUserTable();
    bool createFriendsTable();
    quint32 generateUserId() const;
    quint32 getUserIdByUserName(const QString &userName) const;
    QString getUserNameByUserId(quint32 userId) const;
    bool addFriend(quint32 userId, quint32 friendId);
    QList<QPair<quint32, QString>> getFriendsList(quint32 userId) const;
    QTcpSocket *getChatClient(const QString &account) const;
    QTcpSocket *getClientByUserName(const QString &userName) const;
    QTcpSocket *getClientByUserId(quint32 userId) const;

signals:
    /**
     * @brief Emitted when a new client connects.
     */
    void clientConnected();

    /**
     * @brief Emitted when the user or client list has changed.
     */
    void userListChanged();

    /**
     * @brief Emitted when a message is ready to be read.
     *
     * @param sender The sender of the message.
     * @param message The message content.
     */
    void clientReadyRead(const QString &sender, const QString &message);

private slots:
    void sendResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message);
    void sendAuthResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message, quint32 userId, const QString &userName);
    void sendForwardResponse(QTcpSocket *client, quint16 msgType, bool success, quint32 senderId, const QString &senderName, const QString &senderMessage);
    void sendAddFriendResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message, quint32 userId, const QString &userName);
    void sendDelFriendResponse(QTcpSocket *client, quint16 msgType, bool success, const QString &message, quint32 userId, const QString &userName);
    void broadcastMessage(const QString &sender, const QString &message, QTcpSocket *excludeSocket = nullptr);

    void onNewConnected();
    void onReadyRead();
    void onDisConnected();

    void onHeartbeatTriggered();

private:
    QTcpServer *m_chatServer;                             ///< The TCP server instance.
    QHash<QTcpSocket *, quint32> m_expectedSizeMap;       ///< Maps sockets to expected message sizes for parsing.
    QHash<QTcpSocket *, quint32> m_loggedInUserMap;       ///< Maps sockets to logged-in userIds (quint32).
    QHash<quint32, QPointer<QTcpSocket>> m_userClientMap; ///< Maps userIds (quint32) to bound client sockets.
    QString m_dbPath;                                     ///< SQLite database file path.
    QMutex m_mutex;                                       ///< Mutex for thread-safe access to shared data.
    QThreadPool m_threadPool;                             ///< Thread pool for asynchronous request processing.
    QThread *m_heartbeatThread;                           ///< Thread for heartbeat worker.
    HeartbeatWorker *m_heartbeatWorker;                   ///< Heartbeat worker instance.
};

#endif // NETCHATSERVER_H
