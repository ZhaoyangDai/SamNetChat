#include "netchatview.h"
#include "ui_netchatview.h"
#include "QDateTime"
#include <QListWidgetItem>
#include <QInputDialog>
#include <QMenu>
#include <QAction>

NetChatView::NetChatView(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::NetChatView), m_chatClient(nullptr), m_currentContactId("system")
{
    ui->setupUi(this);
}

NetChatView::~NetChatView()
{
    delete ui;
}

void NetChatView::loadNetChatClient(NetChatClient *chatClient)
{
    m_chatClient = chatClient;

    connect(m_chatClient, &NetChatClient::chatMessageReceived, this, &NetChatView::onChatMsgReceived);
    connect(m_chatClient, &NetChatClient::forwardedMessageReceived, this, &NetChatView::onForwardedMessageReceived);
    connect(m_chatClient, &NetChatClient::forwardMessageResult, this, &NetChatView::onForwardMessageResult);
    connect(m_chatClient, &NetChatClient::addFriendResult, this, &NetChatView::onAddFriendResult);
    connect(m_chatClient, &NetChatClient::contactListReceived, this, &NetChatView::onContactListReceived);
    connect(m_chatClient, &NetChatClient::deleteFriendResult, this, &NetChatView::onDeleteFriendResult);
    connect(m_chatClient, &NetChatClient::serverConnected, this, &NetChatView::onServerConnected);

    ui->contactList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->contactList, &QWidget::customContextMenuRequested, this, &NetChatView::onContactListContextMenuRequested);
    connect(ui->contactList, QOverload<int>::of(&QListWidget::currentRowChanged), this, &NetChatView::onContactSelected);

    m_contacts.clear();
    m_contacts.append({QStringLiteral("system"), QStringLiteral("System")});

    const QList<QPair<quint32, QString>> cachedContacts = m_chatClient->loadCachedContactList();
    if (!cachedContacts.isEmpty())
    {
        onContactListReceived(cachedContacts);
    }
    else
    {
        refreshContactList();
    }

    ui->serverAddrVal->setText(chatClient->getServerAddr());
    ui->serverPortVal->setText(chatClient->getServerPort());

    updateConnectionState();
    updateUserInfo();
    onContactSelected(0);
}

void NetChatView::addContact(const QString &id, const QString &name)
{
    qDebug() << Q_FUNC_INFO << "ID:" << id << " Name:" << name;
    for (const auto &contact : m_contacts)
    {
        if (contact.id == id)
            return;
    }
    m_contacts.append({id, name});
    refreshContactList();
}

void NetChatView::refreshContactList()
{
    qDebug() << Q_FUNC_INFO;
    ui->contactList->clear();
    for (const auto &contact : m_contacts)
    {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(contact.name + "\n(" + contact.id + ")");
        item->setData(Qt::UserRole, contact.id);
        ui->contactList->addItem(item);

        // Set item height to accommodate multi-line text (approx 50px for 2 lines)
        item->setSizeHint(QSize(ui->contactList->width(), 50));
    }
    if (!m_contacts.isEmpty())
    {
        ui->contactList->setCurrentRow(0);
    }
}

void NetChatView::displayMessagesForContact(const QString &contactId)
{
    ui->receiveText->clear();
    if (m_messageHistory.contains(contactId))
    {
        for (const auto &msg : m_messageHistory[contactId])
        {
            // 重建完整格式的消息行
            // 从保存的显示文本中提取完整显示名称（包含时间戳）
            const QString& displayText = msg.first;
            const QString& contentText = msg.second;

            QTextCharFormat format;

            if (contactId == "system")
            {
                // 系统消息格式：字号 12，微软雅黑，橙色
#ifdef Q_OS_WIN
                format.setFontPointSize(12);
                format.setFontFamily("Microsoft YaHei");
#else
                format.setFontPointSize(16);
                format.setFontFamily("Microsoft YaHei");
#endif
                format.setForeground(QColor("#ff9300"));
                format.setBackground(QColor("#fff8e8"));

                QString sysLine = QString("[%1] SYSTEM: %2").arg(displayText).arg(contentText);
                ui->receiveText->append(sysLine);
            }
            else
            {
                // 联系人与私聊消息格式：字号 14，Source Code Pro
                
                // 解析显示文本以确定消息类型
                QString sender;
                QColor textColor;

                // 系统消息（System）用橙色
                if (displayText.startsWith("[") && displayText.contains("] SYSTEM"))
                {
                    sender = "SYSTEM";
                    format.setFontPointSize(14);
                    format.setFontFamily("Source Code Pro");
                    format.setForeground(QColor("#ff9300"));
                }
                // 自己发送的消息（Me）用绿色
                else if (displayText.startsWith("[") && displayText.contains("] Me"))
                {
                    sender = "Me";
                    format.setFontPointSize(14);
                    format.setFontFamily("Source Code Pro");
                    format.setForeground(QColor("#20BF5D"));
                }
                // 其他接收消息用蓝色
                else
                {
                    sender = displayText.section("]", 0, -1).trimmed(); // 获取显示名称
                    format.setFontPointSize(14);
                    format.setFontFamily("Source Code Pro");
                    format.setForeground(QColor("#125e9b"));
                }

                QString line = QString("[%1] %2: %3").arg(displayText).arg(sender.isEmpty() ? "You" : sender).arg(contentText);
                ui->receiveText->setCurrentCharFormat(format);
                ui->receiveText->append(line);
                ui->receiveText->setCurrentCharFormat(QTextCharFormat());
            }
        }
    }
}

void NetChatView::addMessageToContact(const QString &contactId, const QString &sender, const QString &message, bool isPrivate)
{
    // 构建完整格式的消息行：[DateTime] senderName: message
    const QString fullTimestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    
    // 判断消息类型并设置显示标签
    QString displaySender = sender;
    if (sender == "System")
    {
        displaySender = "SYSTEM";
    }
    else if (sender == "Me")
    {
        displaySender = "Me";
    }
    
    // 构建显示文本
    QString displayText = QString("[%1] %2").arg(fullTimestamp, displaySender);
    m_messageHistory[contactId].append({displayText, message});

    // 如果是当前活动的聊天窗口，立即显示
    const bool isActiveContact = (m_currentContactId == contactId);

    if (isActiveContact)
    {
        QTextCharFormat appendFormat;
        appendFormat.setFontPointSize(14);
        
        // 根据发送者设置不同颜色格式
        if (displaySender == "SYSTEM")
        {
            appendFormat.setForeground(QColor("#ff9300"));
        }
        else if (displaySender == "Me")
        {
            appendFormat.setForeground(QColor("#20BF5D"));
        }
        else
        {
            appendFormat.setForeground(QColor("#36C0E3"));
        }

        // 显示使用 append 格式
        ui->receiveText->append(displayText + ":");
        ui->receiveText->setCurrentCharFormat(appendFormat);
        ui->receiveText->append(message);
        ui->receiveText->setCurrentCharFormat(QTextCharFormat());
    }
}

void NetChatView::onServerConnected()
{
    updateConnectionState();
}

void NetChatView::updateConnectionState()
{
    if (!m_chatClient)
    {
        ui->connectState->setText(tr("未连接"));
        return;
    }

    if (m_chatClient->getConnectState() == QAbstractSocket::ConnectedState)
    {
        ui->connectState->setText(tr("已连接"));
    }
    else
    {
        ui->connectState->setText(tr("未连接"));
    }
}

void NetChatView::updateUserInfo()
{
    if (!m_chatClient)
    {
        ui->userIDVal->setText(tr("-"));
        ui->userNameVal->setText(tr("-"));
        return;
    }

    const quint32 userId = m_chatClient->getCurrentUserId();
    const QString userName = m_chatClient->getCurrentUserName();
    ui->userIDVal->setText((userId == 0) ? tr("-") : QString::number(userId));
    ui->userNameVal->setText(userName.isEmpty() ? tr("-") : userName);
}

void NetChatView::onChatMsgReceived(const QString &sender, const QString &message)
{
    addMessageToContact("system", "System", message);
}

void NetChatView::on_sendBtn_clicked()
{
    const QString message = ui->sendText->toPlainText().trimmed();
    if (message.isEmpty())
        return;

    if (m_currentContactId == "system")
    {
        m_chatClient->sendChatMessage(message);
        addMessageToContact("system", "Me", message);
    }
    else if (m_currentContactId == "broadcast")
    {
        m_chatClient->sendChatMessage(message);
        addMessageToContact("broadcast", "Me", message);
    }
    else
    {
        m_chatClient->sendForwardMessage(m_currentContactId, message);
        addMessageToContact(m_currentContactId, "Me", message, true);
    }

    ui->sendText->clear();
}

void NetChatView::onForwardedMessageReceived(const quint32 &fromUserId, const QString &fromUserName, const QString &message)
{
    addMessageToContact(QString::number(fromUserId), fromUserName, message, true);
}

void NetChatView::onForwardMessageResult(bool success, const QString &message)
{
    // const QString status = success ? "[转发成功]" : "[转发失败]";
    // addMessageToContact(m_currentContactId, "System", status + " " + message);
}

void NetChatView::onContactSelected(int row)
{
    if (row < 0 || row >= m_contacts.size())
        return;

    const Contact &contact = m_contacts[row];
    m_currentContactId = contact.id;
    ui->targetAddress->setText(contact.name);
    displayMessagesForContact(contact.id);
}

void NetChatView::on_findFriendBtn_clicked()
{
    if (!m_chatClient || m_chatClient->getConnectState() != QAbstractSocket::ConnectedState)
    {
        ui->receiveText->append("[System] 未连接到服务器");
        return;
    }

    if (0 == m_chatClient->getCurrentUserId())
    {
        ui->receiveText->append("[System] 请先登录");
        return;
    }

    // 创建对话框输入框
    bool ok;
    QString userInput = QInputDialog::getText(this,
                                              tr("添加好友"),
                                              tr("输入用户名或用户ID:"),
                                              QLineEdit::Normal,
                                              QString(),
                                              &ok);

    if (!ok || userInput.isEmpty())
        return;

    // 发送添加好友请求
    m_chatClient->sendAddFriendRequest(userInput);
}

void NetChatView::onAddFriendResult(bool success, const QString &message, const quint32 &userId, const QString &userName)
{
    if (success)
    {
        // 将新好友添加到联系人列表
        addContact(QString::number(userId), userName);
        ui->receiveText->append(QString("[System] ") + QDateTime::currentDateTime().toString());
        ui->receiveText->append(QString("添加好友成功: %1").arg(userName));
    }
    else
    {
        ui->receiveText->append(QString("[System] ") + QDateTime::currentDateTime().toString());
        ui->receiveText->append(QString("添加好友失败: %1").arg(message));
    }
}

void NetChatView::onContactListReceived(const QList<QPair<quint32, QString>> &contacts)
{
    qDebug() << Q_FUNC_INFO << "Received contact list with" << contacts.count() << "contacts";

    // Clear existing contacts except System
    while (m_contacts.size() > 1)
    {
        m_contacts.pop_back();
    }

    // Add all received contacts
    for (const auto &contact : contacts)
    {
        m_contacts.append({QString::number(contact.first), contact.second});
    }

    refreshContactList();

    // Show message in chat window
    if (contacts.count() > 0)
    {
        ui->receiveText->append(QString("[System] ") + QDateTime::currentDateTime().toString());
        ui->receiveText->append(QString("已加载 %1 个联系人").arg(contacts.count()));
    }
}

void NetChatView::onContactListContextMenuRequested(const QPoint &pos)
{
    const QListWidgetItem *item = ui->contactList->itemAt(pos);
    if (!item)
        return;

    const QString contactId = item->data(Qt::UserRole).toString();
    if (contactId == QStringLiteral("system"))
        return;

    QMenu menu(ui->contactList);
    QAction *deleteAction = menu.addAction(tr("删除联系人"));
    QAction *selected = menu.exec(ui->contactList->viewport()->mapToGlobal(pos));
    if (selected == deleteAction)
    {
        bool ok = false;
        quint32 friendId = contactId.toUInt(&ok);
        if (ok && m_chatClient)
        {
            m_chatClient->sendDeleteFriendRequest(friendId);
        }
    }
}

void NetChatView::removeContact(const QString &contactId)
{
    for (int i = 0; i < m_contacts.size(); ++i)
    {
        if (m_contacts[i].id == contactId && m_contacts[i].id != QStringLiteral("system"))
        {
            m_contacts.removeAt(i);
            break;
        }
    }

    refreshContactList();

    QString cacheMsg;
    QList<QPair<quint32, QString>> cacheContacts;
    for (const auto &contact : m_contacts)
    {
        if (contact.id == QStringLiteral("system"))
            continue;
        bool ok = false;
        quint32 id = contact.id.toUInt(&ok);
        if (ok)
        {
            cacheContacts.append(qMakePair(id, contact.name));
        }
    }
    if (m_chatClient)
    {
        m_chatClient->cacheContactList(cacheContacts);
        cacheMsg = tr("联系人已从本地列表中删除。");
    }
    ui->receiveText->append(QString("[System] ") + QDateTime::currentDateTime().toString());
    ui->receiveText->append(cacheMsg);
}

void NetChatView::onDeleteFriendResult(bool success, const QString &message, quint32 friendId)
{
    if (success)
    {
        removeContact(QString::number(friendId));
        ui->receiveText->append(QString("[System] ") + QDateTime::currentDateTime().toString());
        ui->receiveText->append(QString("删除联系人成功"));
    }
    else
    {
        ui->receiveText->append(QString("[System] ") + QDateTime::currentDateTime().toString());
        ui->receiveText->append(QString("删除联系人失败: %1").arg(message));
    }
}

void NetChatView::on_delFriendBtn_clicked()
{
    const QListWidgetItem *item = ui->contactList->currentItem();
    if (!item)
        return;

    const QString contactId = item->data(Qt::UserRole).toString();
    if (contactId == QStringLiteral("system"))
        return;

    bool ok = false;
    quint32 friendId = contactId.toUInt(&ok);
    if (ok && m_chatClient) {
        m_chatClient->sendDeleteFriendRequest(friendId);
    }
}

