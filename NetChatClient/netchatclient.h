#ifndef NETCHATCLIENT_H
#define NETCHATCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QList>
#include <QPair>
#include "protocol.h"

class NetChatClient : public QObject
{
    Q_OBJECT

public:
    NetChatClient(QObject *parent = nullptr);
    ~NetChatClient();

    void connectToServer();

    QString getServerAddr() const;
    QString getServerPort() const;
    int getConnectState() const;
    quint32 getCurrentUserId() const;
    QString getCurrentUserName() const;

    QString getClientAddr(const QString &userName = nullptr) const;

    void sendRegisterRequest(const QString &userName, const QString &password);
    void sendLoginRequest(const QString &userName, const QString &password);
    void sendChatMessage(const QString &message);
    void sendForwardMessage(const QString &targetUserAccount, const QString &message);
    void sendForwardMessageToUserId(quint32 targetUserId, const QString &message);
    void sendAddFriendRequest(const QString &userAccount);
    void sendAddFriendRequestByUserId(quint32 userId);
    void sendDeleteFriendRequest(quint32 friendId);

    QList<QPair<quint32, QString>> loadCachedContactList() const;
    void cacheContactList(const QList<QPair<quint32, QString>> &contacts);

signals:
    void serverConnected();

    void registerResult(bool success, const QString &message, quint32 userId);
    void loginResult(bool success, const QString &message, quint32 userId);
    void chatMessageReceived(const QString &sender, const QString &message);
    void forwardMessageResult(bool success, const QString &message);
    void forwardedMessageReceived(quint32 fromUserId, const QString &fromUserName, const QString &message);
    void addFriendResult(bool success, const QString &message, quint32 userId, const QString &userName);
    void deleteFriendResult(bool success, const QString &message, quint32 friendId);
    void contactListReceived(const QList<QPair<quint32, QString>> &contacts);

private slots:
    void onConnected();
    void onReadyRead();

private:
    QTcpSocket *m_chatClient;
    quint32 m_expectedSize;
    quint32 m_currentUserId;
    QString m_currentUserName;
    QList<QPair<quint32, QString>> m_cachedContacts;
};

#endif // NETCHATCLIENT_H
