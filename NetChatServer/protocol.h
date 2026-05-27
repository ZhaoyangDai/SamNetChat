#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QDataStream>
#include <QByteArray>
#include <QString>
#include <QIODevice>

inline constexpr quint32 kNumericUserIdRequestMarker = 0xF0A1B2C3;

// 消息类型枚举
namespace MsgType
{
    const quint16 ChatMessage = 0;       // 聊天消息
    const quint16 RegisterReq = 1;       // 注册请求
    const quint16 RegisterResp = 2;      // 注册响应
    const quint16 LoginReq = 3;          // 登录请求
    const quint16 LoginResp = 4;         // 登录响应
    const quint16 ForwardMsgReq = 5;     // 转发消息请求 (targetUserAccount or targetUserId, message)
    const quint16 ForwardMsgResp = 6;    // 转发消息响应 (success, message)
    const quint16 AddFriendReq = 7;      // 添加好友请求 (userAccount or userId)
    const quint16 AddFriendResp = 8;     // 添加好友响应 (success, message, userId, userName)
    const quint16 ContactListResp = 9;   // 联系人列表响应 (count, [userId, userName, ...])
    const quint16 DeleteFriendReq = 10;  // 删除好友请求 (friendId)
    const quint16 DeleteFriendResp = 11; // 删除好友响应 (success, message)
}

// 工具函数：将账号密码打包为载荷 (QByteArray)
inline QByteArray packCredentials(const QString &userName, const QString &password)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << userName << password;
    return payload;
}

// 工具函数：将认证结果打包为载荷 (success, message, userId, userName)
inline QByteArray packAuthResponse(bool success, const QString &message, quint32 userId = 0, const QString &userName = QString())
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message << userId << userName;
    return payload;
}

inline void unpackAuthResponse(const QByteArray &payload, bool &success, QString &message, quint32 &userId, QString &userName)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> success >> message >> userId >> userName;
}

// 工具函数：将转发消息请求打包为载荷 (targetUserAccount, message)
inline QByteArray packForwardMsgRequest(const QString &targetUserAccount, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << targetUserAccount << message;
    return payload;
}

inline void unpackForwardMsgRequest(const QByteArray &payload, QString &targetUserAccount, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> targetUserAccount >> message;
}

inline QByteArray packForwardMsgRequestById(quint32 targetUserId, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kNumericUserIdRequestMarker << targetUserId << message;
    return payload;
}

inline void unpackForwardMsgRequestById(const QByteArray &payload, quint32 &targetUserId, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 marker;
    in >> marker;
    if (marker != kNumericUserIdRequestMarker)
        return;
    in >> targetUserId >> message;
}

// 工具函数：将转发消息响应打包为载荷 (success, message)
inline QByteArray packForwardMsgResponse(bool success, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message;
    return payload;
}

inline void unpackForwardMsgResponse(const QByteArray &payload, bool &success, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> success >> message;
}

// 工具函数：将被转发的消息打包为载荷 (fromUserId, fromUserName, message)
inline QByteArray packForwardedMessage(quint32 fromUserId, const QString &fromUserName, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << fromUserId << fromUserName << message;
    return payload;
}

inline void unpackForwardedMessage(const QByteArray &payload, quint32 &fromUserId, QString &fromUserName, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> fromUserId >> fromUserName >> message;
}

// 工具函数：将添加好友请求打包为载荷 (userAccount)
inline QByteArray packAddFriendRequest(const QString &userAccount)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << userAccount;
    return payload;
}

inline void unpackAddFriendRequest(const QByteArray &payload, QString &userAccount)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> userAccount;
}

inline QByteArray packAddFriendRequestById(quint32 userId)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kNumericUserIdRequestMarker << userId;
    return payload;
}

inline void unpackAddFriendRequestById(const QByteArray &payload, quint32 &userId)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 marker;
    in >> marker;
    if (marker != kNumericUserIdRequestMarker)
        return;
    in >> userId;
}

// 工具函数：将添加好友响应打包为载荷 (success, message, userId, userName)
inline QByteArray packAddFriendResponse(bool success, const QString &message, quint32 userId = 0, const QString &userName = QString())
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message << userId << userName;
    return payload;
}

inline void unpackAddFriendResponse(const QByteArray &payload, bool &success, QString &message, quint32 &userId, QString &userName)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> success >> message >> userId >> userName;
}

// 工具函数：将联系人列表打包为载荷 (count, [userId, userName, ...])
inline QByteArray packContactList(const QList<QPair<quint32, QString>> &contacts)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << static_cast<quint32>(contacts.size());
    for (const auto &contact : contacts)
    {
        out << contact.first << contact.second;
    }
    return payload;
}

inline void unpackContactList(const QByteArray &payload, QList<QPair<quint32, QString>> &contacts)
{
    contacts.clear();
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 count;
    in >> count;
    for (quint32 i = 0; i < count; ++i)
    {
        quint32 userId;
        QString userName;
        in >> userId >> userName;
        contacts.append(qMakePair(userId, userName));
    }
}

// 工具函数：将删除好友请求打包为载荷 (friendId)
inline QByteArray packDeleteFriendRequest(quint32 friendId)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << friendId;
    return payload;
}

inline void unpackDeleteFriendRequest(const QByteArray &payload, quint32 &friendId)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> friendId;
}

// 工具函数：将删除好友响应打包为载荷 (success, message)
inline QByteArray packDeleteFriendResponse(bool success, const QString &message, quint32 userId = 0, const QString &userName = QString())
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message << userId << userName;
    return payload;
}

inline void unpackDeleteFriendResponse(const QByteArray &payload, bool &success, QString &message, quint32 &userId, QString &userName)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> success >> message >> userId >> userName;
}

// 工具函数：将消息类型和载荷打包为完整网络包 [长度][类型][载荷]
inline QByteArray packMessage(quint16 msgType, const QByteArray &payload)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(0) << msgType << payload;
    out.device()->seek(0);
    out << quint32(block.size() - sizeof(quint32));
    return block;
}

#endif // PROTOCOL_H
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QDataStream>
#include <QByteArray>
#include <QString>
#include <QIODevice>

inline constexpr quint32 kNumericUserIdRequestMarker = 0xF0A1B2C3;

// 消息类型枚举
namespace MsgType
{
    const quint16 ChatMessage = 0;       // 聊天消息
    const quint16 RegisterReq = 1;       // 注册请求
    const quint16 RegisterResp = 2;      // 注册响应
    const quint16 LoginReq = 3;          // 登录请求
    const quint16 LoginResp = 4;         // 登录响应
    const quint16 ForwardMsgReq = 5;     // 转发消息请求 (targetUserAccount or targetUserId, message)
    const quint16 ForwardMsgResp = 6;    // 转发消息响应 (success, message)
    const quint16 AddFriendReq = 7;      // 添加好友请求 (userAccount or userId)
    const quint16 AddFriendResp = 8;     // 添加好友响应 (success, message, userId, userName)
    const quint16 ContactListResp = 9;   // 联系人列表响应 (count, [userId, userName, ...])
    const quint16 DeleteFriendReq = 10;  // 删除好友请求 (friendId)
    const quint16 DeleteFriendResp = 11; // 删除好友响应 (success, message)
}

// 工具函数：将账号密码打包为载荷 (QByteArray)
inline QByteArray packCredentials(const QString &userName, const QString &password)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << userName << password;
    return payload;
}

// 工具函数：将认证结果打包为载荷 (success, message, userId, userName)
inline QByteArray packAuthResponse(bool success, const QString &message, quint32 userId = 0, const QString &userName = QString())
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message << userId << userName;
    return payload;
}

inline void unpackAuthResponse(const QByteArray &payload, bool &success, QString &message, quint32 &userId, QString &userName)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> success >> message >> userId >> userName;
}

// 工具函数：将转发消息请求打包为载荷 (targetUserAccount, message)
inline QByteArray packForwardMsgRequest(const QString &targetUserAccount, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << targetUserAccount << message;
    return payload;
}

inline void unpackForwardMsgRequest(const QByteArray &payload, QString &targetUserAccount, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> targetUserAccount >> message;
}

inline QByteArray packForwardMsgRequestById(quint32 targetUserId, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kNumericUserIdRequestMarker << targetUserId << message;
    return payload;
}

inline void unpackForwardMsgRequestById(const QByteArray &payload, quint32 &targetUserId, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 marker;
    in >> marker;
    if (marker != kNumericUserIdRequestMarker)
        return;
    in >> targetUserId >> message;
}

// 工具函数：将转发消息响应打包为载荷 (success, message)
inline QByteArray packForwardMsgResponse(bool success, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message;
    return payload;
}

inline void unpackForwardMsgResponse(const QByteArray &payload, bool &success, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> success >> message;
}

// 工具函数：将被转发的消息打包为载荷 (fromUserId, fromUserName, message)
inline QByteArray packForwardedMessage(quint32 fromUserId, const QString &fromUserName, const QString &message)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << fromUserId << fromUserName << message;
    return payload;
}

inline void unpackForwardedMessage(const QByteArray &payload, quint32 &fromUserId, QString &fromUserName, QString &message)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> fromUserId >> fromUserName >> message;
}

// 工具函数：将添加好友请求打包为载荷 (userAccount)
inline QByteArray packAddFriendRequest(const QString &userAccount)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << userAccount;
    return payload;
}

inline void unpackAddFriendRequest(const QByteArray &payload, QString &userAccount)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> userAccount;
}

inline QByteArray packAddFriendRequestById(quint32 userId)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << kNumericUserIdRequestMarker << userId;
    return payload;
}

inline void unpackAddFriendRequestById(const QByteArray &payload, quint32 &userId)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 marker;
    in >> marker;
    if (marker != kNumericUserIdRequestMarker)
        return;
    in >> userId;
}

// 工具函数：将添加好友响应打包为载荷 (success, message, userId, userName)
inline QByteArray packAddFriendResponse(bool success, const QString &message, quint32 userId = 0, const QString &userName = QString())
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << success << message << userId << userName;
    return payload;
}

inline void unpackAddFriendResponse(const QByteArray &payload, bool &success, QString &message, quint32 &userId, QString &userName)
{
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in >> success >> message >> userId >> userName;
}

// 工具函数：将联系人列表打包为载荷 (count, [userId, userName, ...])
inline QByteArray packContactList(const QList<QPair<quint32, QString>> &contacts)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << static_cast<quint32>(contacts.size());
    for (const auto &contact : contacts)
    {
        out << contact.first << contact.second;
    }
    return payload;
}

inline void unpackContactList(const QByteArray &payload, QList<QPair<quint32, QString>> &contacts)
{
    contacts.clear();
    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 count;
    in >> count;
    for (quint32 i = 0; i < count; ++i)
    {
        quint32 userId;
        QString userName;
        in >> userId >> userName;
        contacts.append(qMakePair(userId, userName));
    }
}

// 工具函数：将消息类型和载荷打包为完整网络包 [长度][类型][载荷]
inline QByteArray packMessage(quint16 msgType, const QByteArray &payload)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(0) << msgType << payload;
    out.device()->seek(0);
    out << quint32(block.size() - sizeof(quint32));
    return block;
}

#endif // PROTOCOL_H
