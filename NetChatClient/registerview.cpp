#include "registerview.h"
#include "ui_registerview.h"
#include <QMessageBox>
#include <QLabel>

RegisterView::RegisterView(QWidget *parent)
    : QWidget(parent), ui(new Ui::RegisterView)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Window);
    setWindowTitle(tr("SimplyNetChat - 注册"));

    setupUi();
    initConnections();
}

RegisterView::~RegisterView()
{
    delete ui;
}

void RegisterView::showErrorMessage(const QString &message)
{
    QMessageBox::warning(this, tr("注册错误"), message);
}

void RegisterView::setupUi()
{
    // 设置密码输入框为隐藏模式
    ui->userPasswordVal->setEchoMode(QLineEdit::Password);
    ui->userConfirmeVal->setEchoMode(QLineEdit::Password);

    // 清除所有输入框
    ui->userNameVal->clear();
    ui->userPasswordVal->clear();
    ui->userConfirmeVal->clear();

    // 设置输入框提示
    ui->userNameVal->setPlaceholderText(tr("请输入用户名（至少3个字符）"));
    ui->userPasswordVal->setPlaceholderText(tr("请输入密码（至少3个字符）"));
    ui->userConfirmeVal->setPlaceholderText(tr("请再次输入密码"));

    // 设置默认焦点
    ui->userNameVal->setFocus();

    // 初始时禁用确认按钮
    ui->confirmeBtn->setEnabled(false);
}

void RegisterView::initConnections()
{
    // 输入变化时更新按钮状态
    connect(ui->userNameVal, &QLineEdit::textChanged, this, &RegisterView::onUserNameChanged);
    connect(ui->userPasswordVal, &QLineEdit::textChanged, this, &RegisterView::onPasswordChanged);
    connect(ui->userConfirmeVal, &QLineEdit::textChanged, this, &RegisterView::onPasswordConfirmChanged);

    // 按Enter键注册
    connect(ui->userConfirmeVal, &QLineEdit::returnPressed, ui->confirmeBtn, &QPushButton::click);
}

bool RegisterView::validateInput()
{
    const QString username = ui->userNameVal->text().trimmed();
    const QString password = ui->userPasswordVal->text();
    const QString confirmPassword = ui->userConfirmeVal->text();

    if (username.isEmpty())
    {
        showErrorMessage(tr("用户名不能为空"));
        ui->userNameVal->setFocus();
        return false;
    }

    if (username.length() < 3)
    {
        showErrorMessage(tr("用户名长度至少3个字符"));
        ui->userNameVal->setFocus();
        return false;
    }

    if (username.length() > 20)
    {
        showErrorMessage(tr("用户名长度不能超过20个字符"));
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

    if (password != confirmPassword)
    {
        showErrorMessage(tr("两次输入的密码不一致"));
        ui->userConfirmeVal->clear();
        ui->userConfirmeVal->setFocus();
        return false;
    }

    return true;
}

void RegisterView::updateRegisterButtonState()
{
    bool hasUsername = !ui->userNameVal->text().trimmed().isEmpty();
    bool hasPassword = !ui->userPasswordVal->text().isEmpty();
    bool hasConfirm = !ui->userConfirmeVal->text().isEmpty();

    ui->confirmeBtn->setEnabled(hasUsername && hasPassword && hasConfirm);
}

void RegisterView::on_confirmeBtn_clicked()
{
    if (!validateInput())
        return;

    emit confirmeRegister(ui->userNameVal->text().trimmed(), ui->userPasswordVal->text());
    ui->confirmeBtn->setEnabled(false);
    ui->confirmeBtn->setText(tr("注册中..."));
}

void RegisterView::on_cancelBtn_clicked()
{
    emit cancelRegister();
}

void RegisterView::onUserNameChanged(const QString &text)
{
    Q_UNUSED(text);
    updateRegisterButtonState();
}

void RegisterView::onPasswordChanged(const QString &text)
{
    Q_UNUSED(text);
    updateRegisterButtonState();
}

void RegisterView::onPasswordConfirmChanged(const QString &text)
{
    Q_UNUSED(text);
    updateRegisterButtonState();
}
