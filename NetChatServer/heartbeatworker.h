#ifndef HEARTBEATWORKER_H
#define HEARTBEATWORKER_H

#include <QObject>
#include <QTimer>

class HeartbeatWorker : public QObject {
    Q_OBJECT
public:
    explicit HeartbeatWorker(QObject *parent = nullptr);

public slots:
    // 将在子线程中被调用，用于启动定时器
    void startHeartbeat();

signals:
    // 当定时器超时，通知主线程发送心跳包
    void heartbeatTriggered();

private:
    QTimer *m_timer;
};

#endif // HEARTBEATWORKER_H
