#ifndef NETCHATVIEW_H
#define NETCHATVIEW_H

#include "netchatserver.h"
#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class NetChatView;
}
QT_END_NAMESPACE

class NetChatView : public QWidget
{
    Q_OBJECT

public:
    explicit NetChatView(QWidget *parent = nullptr);
    ~NetChatView();

    void loadNetChatServer(NetChatServer *chatServer);

private slots:
    void onNewConnected();
    void onReadyRead(const QString &sender, const QString &message);

    void refreshClientList();

    void on_pushBtn_clicked();
    void on_logsPushBtn_clicked();
    void on_usersRefreshBtn_clicked();
    void on_settingsUpgradeBtn_clicked();
    void onDeleteUserButtonClicked(quint32 userId);

    void on_refreshBtn_clicked();

private:
    void appendServerLog(const QString &message);
    void updateServerInfoDisplay();
    void sendServerPushMessage(const QString &message);

    Ui::NetChatView *ui;
    NetChatServer *m_chatServer;
};
#endif // NETCHATVIEW_H
