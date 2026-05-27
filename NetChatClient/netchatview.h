#ifndef NETCHATVIEW_H
#define NETCHATVIEW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTextCharFormat>
#include <QMap>
#include <QPair>
#include <QFont>
#include "netchatclient.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class NetChatView;
}
QT_END_NAMESPACE

struct Contact
{
    QString id;
    QString name;
};

class NetChatView : public QMainWindow
{
    Q_OBJECT

  private:
    QString m_currentUserFullname;

public:
    NetChatView(QWidget *parent = nullptr);
    ~NetChatView();

    void loadNetChatClient(NetChatClient *chatClient);
    void addContact(const QString &id, const QString &name);

private slots:
    void onChatMsgReceived(const QString &sender, const QString &message);
    void onForwardedMessageReceived(const quint32 &fromUserId, const QString &fromUserName, const QString &message);
    void onForwardMessageResult(bool success, const QString &message);
    void onAddFriendResult(bool success, const QString &message, const quint32 &userId, const QString &userName);
    void onContactListReceived(const QList<QPair<quint32, QString>> &contacts);
    void onServerConnected();
    void onContactSelected(int row);
    void on_findFriendBtn_clicked();
    void onContactListContextMenuRequested(const QPoint &pos);
    void onDeleteFriendResult(bool success, const QString &message, quint32 friendId);

    void on_sendBtn_clicked();

    void on_delFriendBtn_clicked();

private:
    void updateConnectionState();
    void updateUserInfo();
    void refreshContactList();

    /**
     * Format: [DateTime] senderName: message
     * System messages: 16pt, Microsoft YaHei, #ff9300
     * Received messages: 14pt, Source Code Pro, #125e9b  
     * Sent messages: 14pt, Source Code Pro, #20BF5D
     * 
     * @param contactId Contact identifier
     * @param sender "System", "Me", or other contact name
     * @param message Message content
     */
    void displayMessagesForContact(const QString &contactId);
    void addMessageToContact(const QString &contactId, const QString &sender, const QString &message, bool isPrivate = false);
    void removeContact(const QString &contactId);

    Ui::NetChatView *ui;
    NetChatClient *m_chatClient;
    QString m_currentContactId;
    QMap<QString, QList<QPair<QString, QString>>> m_messageHistory; // contactId -> list of (sender, message)
    QList<Contact> m_contacts;
};
#endif // NETCHATVIEW_H
