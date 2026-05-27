#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include "netchatclient.h"
#include "registerview.h"

namespace Ui
{
    class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    void loadNetChatClient(NetChatClient *chatClient);

private slots:
    void onServerConnected();

    void onConfirmeRegister(const QString &userName, const QString &password);
    void onCancelRegister();

    void onLoginResult(bool success, const QString &message, const quint32 &userId);
    void onRegisterResult(bool success, const QString &message, const quint32 &userId);

    void on_loginBtn_clicked();
    void on_registerBtn_clicked();

    void on_cancelBtn_clicked();

    void onUserNameChanged(const QString &text);
    void onPasswordChanged(const QString &text);

private:
    void setupUi();
    void initConnections();
    bool validateInput();
    void updateLoginButtonState();
    void clearErrorMessage();
    void showErrorMessage(const QString &message);

    Ui::LoginDialog *ui;

    int m_errorCode;
    NetChatClient *m_chatClient;
    RegisterView *m_registerView;
};

#endif // LOGINDIALOG_H
