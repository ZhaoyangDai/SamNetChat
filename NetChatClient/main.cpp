#include "netchatclient.h"
#include "netchatview.h"
#include "logindialog.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    NetChatClient chatClient;
    chatClient.connectToServer();

    NetChatView chatView;

    LoginDialog loginDlg;
    loginDlg.loadNetChatClient(&chatClient);

    int ret = loginDlg.exec();

    if (QDialog::Accepted == ret) {
        chatView.loadNetChatClient(&chatClient);
        chatView.show();
    }

    return a.exec();
}
