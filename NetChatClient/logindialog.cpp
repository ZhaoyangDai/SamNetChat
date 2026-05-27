#include "logindialog.h"
#include "ui_logindialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QKeyEvent>
#include <QLabel>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LoginDialog), m_chatClient(nullptr), m_registerView(nullptr)
{
    ui->setupUi(this);

    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("SimplyNetChat - 登陆"));

    setupUi();
    initConnections();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::setupUi()
{
    // 设置密码输入框为隐藏模式
    ui->userPasswordVal->setEchoMode(QLineEdit::Password);

    // 清除测试数据
    ui->userNameVal->clear();
    ui->userPasswordVal->clear();

    // 设置输入框提示
    ui->userNameVal->setPlaceholderText(tr("请输入用户名"));
    ui->userPasswordVal->setPlaceholderText(tr("请输入密码"));

    // 设置默认焦点
    ui->userNameVal->setFocus();

    // 按钮重新排列顺序：登陆、注册、取消
    ui->loginBtn->setDefault(true);

    // 初始时禁用登陆按钮（需要输入用户名和密码）
    ui->loginBtn->setEnabled(false);
}

void LoginDialog::initConnections()
{
    // 输入变化时更新按钮状态
    connect(ui->userNameVal, &QLineEdit::textChanged, this, &LoginDialog::onUserNameChanged);
    connect(ui->userPasswordVal, &QLineEdit::textChanged, this, &LoginDialog::onPasswordChanged);

    // 按Enter键登陆
    connect(ui->userPasswordVal, &QLineEdit::returnPressed, ui->loginBtn, &QPushButton::click);
}

bool LoginDialog::validateInput()
{
    const QString userName = ui->userNameVal->text().trimmed();
    const QString password = ui->userPasswordVal->text();

    if (userName.isEmpty())
    {
        showErrorMessage(tr("用户名不能为空"));
        ui->userNameVal->setFocus();
        return false;
    }

    if (userName.length() < 3)
    {
        showErrorMessage(tr("用户名长度至少3个字符"));
        ui->userNameVal->setFocus();
        return false;
    }

    if (password.isEmpty())
    {
        showErrorMessage(tr("密码不能为空"));
        ui->userPasswordVal->setFocus();
        return false;
    }

    if (password.length() < 3)
    {
        showErrorMessage(tr("密码长度至少3个字符"));
        ui->userPasswordVal->setFocus();
        return false;
    }

    clearErrorMessage();
    return true;
}

void LoginDialog::updateLoginButtonState()
{
    bool hasUsername = !ui->userNameVal->text().trimmed().isEmpty();
    bool hasPassword = !ui->userPasswordVal->text().isEmpty();
    ui->loginBtn->setEnabled(hasUsername && hasPassword);
}

void LoginDialog::clearErrorMessage()
{
    // 如果您添加了错误标签，在这里清除
    // ui->errorMessage->clear();
}

void LoginDialog::showErrorMessage(const QString &message)
{
    QMessageBox::warning(this, tr("输入错误"), message);
}

void LoginDialog::loadNetChatClient(NetChatClient *chatClient)
{
    qDebug() << Q_FUNC_INFO << " load chat client.";
    m_chatClient = chatClient;

    connect(m_chatClient, &NetChatClient::serverConnected, this, &LoginDialog::onServerConnected);
    connect(m_chatClient, &NetChatClient::loginResult, this, &LoginDialog::onLoginResult);
    connect(m_chatClient, &NetChatClient::registerResult, this, &LoginDialog::onRegisterResult);

    // if (m_chatClient->getConnectState() == QAbstractSocket::ConnectedState) {
    //     ui->connectedState->setText("已连接");
    // }
}

void LoginDialog::onServerConnected()
{
    ui->connectStateVal->setText("已连接");
}

void LoginDialog::onConfirmeRegister(const QString &userName, const QString &password)
{
    qDebug() << Q_FUNC_INFO << "user:" << userName << " password:" << password;
    m_chatClient->sendRegisterRequest(userName, password);
}

void LoginDialog::onCancelRegister()
{
    m_registerView->hide();
}

void LoginDialog::onLoginResult(bool success, const QString &message, const quint32 &userId)
{
    qDebug() << Q_FUNC_INFO << "Result: " << success << " Msg: " << message << " userId:" << userId;

    ui->loginBtn->setEnabled(true);
    ui->loginBtn->setText(tr("登陆"));

    if (success)
    {
        delete m_registerView;
        m_registerView = nullptr;
        accept();
    }
    else
    {
        showErrorMessage(message);
        ui->userPasswordVal->clear();
        ui->userPasswordVal->setFocus();
    }
}

void LoginDialog::onRegisterResult(bool success, const QString &message, const quint32 &userId)
{
    qDebug() << Q_FUNC_INFO << "Result: " << success << " Msg: " << message << " userId:" << userId;
    if (success)
    {
        QMessageBox::information(this, tr("注册成功"),
                                 tr("%1\n您的账号ID: %2\n请使用ID或用户名登陆").arg(message, userId));
        if (m_registerView)
        {
            m_registerView->hide();
        }
    }
    else
    {
        if (m_registerView)
        {
            m_registerView->showErrorMessage(message);
        }
    }
}

void LoginDialog::onUserNameChanged(const QString &text)
{
    Q_UNUSED(text);
    updateLoginButtonState();
}

void LoginDialog::onPasswordChanged(const QString &text)
{
    Q_UNUSED(text);
    updateLoginButtonState();
}

void LoginDialog::on_loginBtn_clicked()
{
    if (!validateInput())
        return;

    m_chatClient->sendLoginRequest(ui->userNameVal->text().trimmed(), ui->userPasswordVal->text());
    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText(tr("登陆中..."));
}

void LoginDialog::on_registerBtn_clicked()
{
    m_registerView = new RegisterView();
    connect(m_registerView, &RegisterView::confirmeRegister, this, &LoginDialog::onConfirmeRegister);
    connect(m_registerView, &RegisterView::cancelRegister, this, &LoginDialog::onCancelRegister);

    m_registerView->show();
}

void LoginDialog::on_cancelBtn_clicked()
{
    reject();
}
