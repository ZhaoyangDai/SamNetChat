#include "netchatclient.h"
#include <QDebug>
#include <QSettings>

NetChatClient::NetChatClient(QObject *parent)
    : m_expectedSize(0), m_currentUserId(0)
{
    m_cachedContacts = loadCachedContactList();
}

NetChatClient::~NetChatClient()
{
}

QList<QPair<quint32, QString>> NetChatClient::loadCachedContactList() const
{
    QList<QPair<quint32, QString>> contacts;
    QSettings settings(QStringLiteral("SeventProject"), QStringLiteral("SimplyNetChat"));
    const QVariantList savedContacts = settings.value(QStringLiteral("contacts")).toList();
    for (const QVariant &item : savedContacts)
    {
        const QVariantMap map = item.toMap();
        const quint32 userId = static_cast<quint32>(map.value(QStringLiteral("userId")).toUInt());
        const QString userName = map.value(QStringLiteral("userName")).toString();
        if (userId != 0 && !userName.isEmpty())
        {
            contacts.append(qMakePair(userId, userName));
        }
    }
    return contacts;
}

void NetChatClient::cacheContactList(const QList<QPair<quint32, QString>> &contacts)
{
    m_cachedContacts = contacts;
    QVariantList savedContacts;
    for (const auto &contact : contacts)
    {
        QVariantMap map;
        map.insert(QStringLiteral("userId"), contact.first);
        map.insert(QStringLiteral("userName"), contact.second);
        savedContacts.append(map);
    }

    QSettings settings(QStringLiteral("SeventProject"), QStringLiteral("SimplyNetChat"));
    settings.setValue(QStringLiteral("contacts"), savedContacts);
}

void NetChatClient::connectToServer()
{
    m_chatClient = new QTcpSocket();

    connect(m_chatClient, &QTcpSocket::connected, this, &NetChatClient::onConnected);
    connect(m_chatClient, &QTcpSocket::readyRead, this, &NetChatClient::onReadyRead);

    QString ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
    QString portAddress = "6688";

    // Connect to Server
    m_chatClient->connectToHost(ipAddress, portAddress.toInt());
}

QString NetChatClient::getServerAddr() const
{
    return m_chatClient->localAddress().toString();
}

QString NetChatClient::getServerPort() const
{
    return "6688";
}

int NetChatClient::getConnectState() const
{
    return m_chatClient->state();
}

quint32 NetChatClient::getCurrentUserId() const
{
    return m_currentUserId;
}

QString NetChatClient::getCurrentUserName() const
{
    return m_currentUserName;
}

void NetChatClient::sendRegisterRequest(const QString &userName, const QString &password)
{
    qDebug() << Q_FUNC_INFO << "user: " << userName << " password: " << password;
    if (!m_chatClient)
        return;
    QByteArray payload = packCredentials(userName, password);
    m_chatClient->write(packMessage(MsgType::RegisterReq, payload));
}

void NetChatClient::sendLoginRequest(const QString &userName, const QString &password)
{
    qDebug() << Q_FUNC_INFO << "user: " << userName << " password: " << password;
    if (!m_chatClient)
        return;
    QByteArray payload = packCredentials(userName, password);
    m_chatClient->write(packMessage(MsgType::LoginReq, payload));
}

void NetChatClient::sendChatMessage(const QString &message)
{
    qDebug() << Q_FUNC_INFO << "MSG: " << message;
    if (!m_chatClient)
        return;
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << message;
    m_chatClient->write(packMessage(MsgType::ChatMessage, payload));
}

void NetChatClient::sendForwardMessage(const QString &targetUserAccount, const QString &message)
{
    qDebug() << Q_FUNC_INFO << "Target:" << targetUserAccount << "MSG:" << message;
    if (!m_chatClient)
        return;
    QByteArray payload = packForwardMsgRequest(targetUserAccount, message);
    m_chatClient->write(packMessage(MsgType::ForwardMsgReq, payload));
}

void NetChatClient::sendForwardMessageToUserId(quint32 targetUserId, const QString &message)
{
    qDebug() << Q_FUNC_INFO << "Target userId:" << targetUserId << "MSG:" << message;
    if (!m_chatClient)
        return;
    QByteArray payload = packForwardMsgRequestById(targetUserId, message);
    m_chatClient->write(packMessage(MsgType::ForwardMsgReq, payload));
}

void NetChatClient::sendAddFriendRequest(const QString &userAccount)
{
    qDebug() << Q_FUNC_INFO << "Friend request for:" << userAccount;
    if (!m_chatClient)
        return;
    QByteArray payload = packAddFriendRequest(userAccount);
    m_chatClient->write(packMessage(MsgType::AddFriendReq, payload));
}

void NetChatClient::sendAddFriendRequestByUserId(quint32 userId)
{
    qDebug() << Q_FUNC_INFO << "Friend request for userId:" << userId;
    if (!m_chatClient)
        return;
    QByteArray payload = packAddFriendRequestById(userId);
    m_chatClient->write(packMessage(MsgType::AddFriendReq, payload));
}

void NetChatClient::sendDeleteFriendRequest(quint32 friendId)
{
    qDebug() << Q_FUNC_INFO << "Delete friend request for friendId:" << friendId;
    if (!m_chatClient)
        return;
    QByteArray payload = packDeleteFriendRequest(friendId);
    m_chatClient->write(packMessage(MsgType::DeleteFriendReq, payload));
}

void NetChatClient::onConnected()
{
    emit serverConnected();
}

void NetChatClient::onReadyRead()
{
    qDebug() << Q_FUNC_INFO;
    QDataStream in(m_chatClient);
    in.setVersion(QDataStream::Qt_6_0);

    while (true)
    {
        if (m_expectedSize == 0)
        {
            if (m_chatClient->bytesAvailable() < sizeof(quint32))
                break;
            in >> m_expectedSize;
        }

        if (m_chatClient->bytesAvailable() < m_expectedSize)
            break;

        quint16 msgType;
        QByteArray payload;
        in >> msgType >> payload;
        m_expectedSize = 0;

        QDataStream payloadStream(payload);
        payloadStream.setVersion(QDataStream::Qt_6_0);

        switch (msgType)
        {
        case MsgType::RegisterResp:
        {
            bool success;
            QString msg;
            quint32 userId;
            QString username;
            unpackAuthResponse(payload, success, msg, userId, username);
            if (success)
            {
                m_currentUserId = userId;
                m_currentUserName = username;
            }
            qDebug() << Q_FUNC_INFO << "Register Resp:" << msg << "userId:" << userId << "username:" << username;
            emit registerResult(success, msg, userId);
            break;
        }
        case MsgType::LoginResp:
        {
            bool success;
            QString msg;
            quint32 userId;
            QString username;
            unpackAuthResponse(payload, success, msg, userId, username);
            if (success)
            {
                m_currentUserId = userId;
                m_currentUserName = username;
            }
            qDebug() << Q_FUNC_INFO << "Login Resp:" << msg << "userId:" << userId << "username:" << username;
            emit loginResult(success, msg, userId);
            break;
        }
        case MsgType::ChatMessage:
        {
            // 服务端可能发来未登录拦截提示(sender为空) 或 正常聊天消息
            // 我们统一用chatMessageReceived信号抛出
            // 兼容只有message没有sender的异常包（如心跳包当前也被塞进了Chat类型）
            QString part1, part2;
            payloadStream >> part1;
            if (payloadStream.atEnd())
            {
                qDebug() << Q_FUNC_INFO << "Chat >" << "[System] : " << part1;
                emit chatMessageReceived("System", part1);
            }
            else
            {
                payloadStream >> part2;
                qDebug() << Q_FUNC_INFO << "Chat >" << "[" << part1 << "] : " << part2;
                emit chatMessageReceived(part1, part2);
            }

            break;
        }
        case MsgType::ForwardMsgResp:
        {
            bool success;
            QString msg;
            unpackForwardMsgResponse(payload, success, msg);
            qDebug() << Q_FUNC_INFO << "Forward Resp: success=" << success << "msg=" << msg;
            emit forwardMessageResult(success, msg);
            break;
        }
        case MsgType::ForwardMsgReq:
        {
            // 客户端接收被转发的消息：fromUserId=发送者userId, fromUserName=发送者用户名, message=消息内容
            // 解析格式：[fromUserId][fromUserName][message]
            quint32 fromUserId;
            QString fromUserName;
            QString message;
            payloadStream >> fromUserId >> fromUserName >> message;
            qDebug() << Q_FUNC_INFO << "Forwarded from userId:" << fromUserId << "userName:" << fromUserName << "msg:" << message;
            emit forwardedMessageReceived(fromUserId, fromUserName, message);
            break;
        }
        case MsgType::AddFriendResp:
        {
            bool success;
            QString msg;
            quint32 userId;
            QString userName;
            unpackAddFriendResponse(payload, success, msg, userId, userName);
            qDebug() << Q_FUNC_INFO << "Add friend result: success=" << success << "msg=" << msg << "userId=" << userId << "userName=" << userName;
            emit addFriendResult(success, msg, userId, userName);
            break;
        }
        case MsgType::ContactListResp:
        {
            QList<QPair<quint32, QString>> contacts;
            unpackContactList(payload, contacts);
            qDebug() << Q_FUNC_INFO << "Contact list received, count=" << contacts.count();
            for (const auto &contact : contacts)
            {
                qDebug() << "  Contact: userId=" << contact.first << "name=" << contact.second;
            }
            cacheContactList(contacts);
            // emit contactListReceived(contacts);
            qDebug() << Q_FUNC_INFO << "Signal emit success";
            break;
        }
        case MsgType::DeleteFriendResp:
        {
            bool success;
            QString msg;
            quint32 userId = 0;
            QString userName;
            unpackDeleteFriendResponse(payload, success, msg, userId, userName);
            qDebug() << Q_FUNC_INFO << "Delete friend result: success" << success << "msg=" << msg << "userId=" << userId << "userName=" << userName;
            emit deleteFriendResult(success, msg, userId);
            break;
        }
        }
    }
}
