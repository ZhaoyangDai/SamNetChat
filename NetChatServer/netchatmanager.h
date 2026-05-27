#ifndef NETCHATMANAGER_H
#define NETCHATMANAGER_H

#include <QObject>
#include "netchatserver.h"
#include "netchatview.h"

class NetChatManager : QObject
{
    Q_OBJECT
public:
    NetChatManager(QObject *parent = nullptr);
    ~NetChatManager();

    void startChatServer();

private:
    NetChatServer *m_chatServer;
    NetChatView *m_chatView;

};

#endif // NETCHATMANAGER_H
