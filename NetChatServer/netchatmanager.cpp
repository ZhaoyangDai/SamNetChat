#include "netchatmanager.h"

NetChatManager::NetChatManager(QObject *parent)
    : QObject(parent)
    , m_chatServer()
    , m_chatView()
{
}

NetChatManager::~NetChatManager()
{

}

void NetChatManager::startChatServer()
{
    m_chatServer = new NetChatServer();
    m_chatView = new NetChatView();
    m_chatView->loadNetChatServer(m_chatServer);
    m_chatView->show();
}
