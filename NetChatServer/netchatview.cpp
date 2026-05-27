#include "netchatview.h"
#include "ui_netchatview.h"
#include <QNetworkInterface>
#include <QDateTime>
#include <QPushButton>
#include <QTableWidgetItem>

NetChatView::NetChatView(QWidget *parent)
    : QWidget(parent), ui(new Ui::NetChatView), m_chatServer(nullptr)
{
    ui->setupUi(this);
}

NetChatView::~NetChatView()
{
    delete ui;
}

void NetChatView::loadNetChatServer(NetChatServer *chatServer)
{
    m_chatServer = chatServer;
    connect(m_chatServer, &NetChatServer::clientConnected, this, &NetChatView::onNewConnected);
    connect(m_chatServer, &NetChatServer::clientReadyRead, this, &NetChatView::onReadyRead);
    connect(m_chatServer, &NetChatServer::userListChanged, this, &NetChatView::refreshClientList);

    updateServerInfoDisplay();

    const QStringList headers = {QStringLiteral("用户ID"), QStringLiteral("用户名"), QStringLiteral("在线"), QStringLiteral("绑定客户端")};
    ui->clientList->setColumnCount(headers.size());
    ui->clientList->setHorizontalHeaderLabels(headers);
    ui->clientList->horizontalHeader()->setStretchLastSection(true);
    ui->clientList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->clientList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QStringList managementHeaders = headers;
    managementHeaders.append(QStringLiteral("操作"));
    ui->usersClientList->setColumnCount(managementHeaders.size());
    ui->usersClientList->setHorizontalHeaderLabels(managementHeaders);
    ui->usersClientList->horizontalHeader()->setStretchLastSection(true);
    ui->usersClientList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->usersClientList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    refreshClientList();
}

void NetChatView::onNewConnected()
{
    const QString message = QDateTime::currentDateTime().toString() + " New Client Connected.";
    appendServerLog(message);
    refreshClientList();
}

void NetChatView::onReadyRead(const QString &sender, const QString &message)
{
    const QString logLine = QDateTime::currentDateTime().toString() + " From " + sender + ":";
    qDebug() << "New Chat Message Received.";
    appendServerLog(logLine);
    appendServerLog(message);
}

void NetChatView::on_pushBtn_clicked()
{
    const QString serverMsg = ui->serverMsgText->toPlainText().trimmed();
    if (serverMsg.isEmpty() || !m_chatServer)
        return;

    sendServerPushMessage(serverMsg);
    ui->serverMsgText->clear();
}

void NetChatView::on_logsPushBtn_clicked()
{
    const QString serverMsg = ui->logsServerMsgText->toPlainText().trimmed();
    if (serverMsg.isEmpty() || !m_chatServer)
        return;

    sendServerPushMessage(serverMsg);
    ui->logsServerMsgText->clear();
}

void NetChatView::on_usersRefreshBtn_clicked()
{
    refreshClientList();
}

void NetChatView::on_settingsUpgradeBtn_clicked()
{
    const QString tip = QDateTime::currentDateTime().toString() + " Upgrade feature not implemented.";
    appendServerLog(tip);
}

void NetChatView::refreshClientList()
{
    if (!m_chatServer)
        return;

    ui->clientList->setRowCount(0);
    ui->usersClientList->setRowCount(0);
    const QVector<QPair<quint32, QString>> userRecords = m_chatServer->getAllUserRecords();

    for (const auto &record : userRecords)
    {
        const quint32 userId = record.first;
        const QString userName = record.second;
        qDebug() << Q_FUNC_INFO << "ID: " << userId << " Name: " << userName;
        QTcpSocket *boundClient = m_chatServer->getClientByAccount(QString::number(userId));
        const bool isOnline = boundClient != nullptr;
        const QString peerInfo = isOnline ? QStringLiteral("%1:%2").arg(boundClient->peerAddress().toString()).arg(boundClient->peerPort()) : QStringLiteral("-");
        const QString onlineText = isOnline ? QStringLiteral("是") : QStringLiteral("否");

        const int rowHome = ui->clientList->rowCount();
        ui->clientList->insertRow(rowHome);
        ui->clientList->setItem(rowHome, 0, new QTableWidgetItem(QString::number(userId)));
        ui->clientList->setItem(rowHome, 1, new QTableWidgetItem(userName));
        ui->clientList->setItem(rowHome, 2, new QTableWidgetItem(onlineText));
        ui->clientList->setItem(rowHome, 3, new QTableWidgetItem(peerInfo));

        const int rowUsers = ui->usersClientList->rowCount();
        ui->usersClientList->insertRow(rowUsers);
        ui->usersClientList->setItem(rowUsers, 0, new QTableWidgetItem(QString::number(userId)));
        ui->usersClientList->setItem(rowUsers, 1, new QTableWidgetItem(userName));
        ui->usersClientList->setItem(rowUsers, 2, new QTableWidgetItem(onlineText));
        ui->usersClientList->setItem(rowUsers, 3, new QTableWidgetItem(peerInfo));

        QPushButton *deleteButton = new QPushButton(tr("删除"));
        deleteButton->setEnabled(!isOnline);
        deleteButton->setProperty("userId", userId);
        if (isOnline)
        {
            deleteButton->setToolTip(tr("在线用户无法删除。"));
        }
        connect(deleteButton, &QPushButton::clicked, this, [this, userId]()
                { onDeleteUserButtonClicked(userId); });
        ui->usersClientList->setCellWidget(rowUsers, 4, deleteButton);
    }
}

void NetChatView::on_refreshBtn_clicked()
{
    refreshClientList();
}

void NetChatView::onDeleteUserButtonClicked(quint32 userId)
{
    if (!m_chatServer)
        return;

    const bool deleted = m_chatServer->deleteOfflineUser(userId);
    if (deleted)
    {
        const QString message = QStringLiteral("Offline user deleted: %1").arg(userId);
        appendServerLog(message);
        refreshClientList();
    }
    else
    {
        const QString message = QStringLiteral("Failed to delete user: %1. Ensure the user is offline.").arg(userId);
        appendServerLog(message);
    }
}

void NetChatView::appendServerLog(const QString &message)
{
    ui->serverLogText->append(message);
    ui->logsServerLogText->append(message);
}

void NetChatView::updateServerInfoDisplay()
{
    if (!m_chatServer)
        return;

    const QString serverAddr = m_chatServer->getServerAddress();
    const QString serverPort = m_chatServer->getServerPort();

    ui->serverAddrVal->setText(serverAddr);
    ui->serverPortVal->setText(serverPort);
    ui->usersServerAddrVal->setText(serverAddr);
    ui->usersServerPortVal->setText(serverPort);
    ui->settingsServerAddrVal->setText(serverAddr);
    ui->settingsServerPortVal->setText(serverPort);
}

void NetChatView::sendServerPushMessage(const QString &message)
{
    if (!m_chatServer)
        return;

    m_chatServer->sendChatMessage("Server", message);
    const QString logLine = QDateTime::currentDateTime().toString() + " Server push:";
    appendServerLog(logLine);
    appendServerLog(message);
}
