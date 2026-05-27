#ifndef REGISTERVIEW_H
#define REGISTERVIEW_H

#include <QWidget>

namespace Ui
{
    class RegisterView;
}

class RegisterView : public QWidget
{
    Q_OBJECT

public:
    explicit RegisterView(QWidget *parent = nullptr);
    ~RegisterView();

    void showErrorMessage(const QString &message);

signals:
    void confirmeRegister(const QString &userName, const QString &password);
    void cancelRegister();

private slots:
    void on_confirmeBtn_clicked();

    void on_cancelBtn_clicked();

    void onUserNameChanged(const QString &text);
    void onPasswordChanged(const QString &text);
    void onPasswordConfirmChanged(const QString &text);

private:
    void setupUi();
    void initConnections();
    bool validateInput();
    void updateRegisterButtonState();

    Ui::RegisterView *ui;
};

#endif // REGISTERVIEW_H
