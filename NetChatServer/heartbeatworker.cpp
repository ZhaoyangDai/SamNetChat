#include "HeartbeatWorker.h"
#include <QDebug>
HeartbeatWorker::HeartbeatWorker(QObject *parent) : QObject(parent), m_timer(nullptr) {}

void HeartbeatWorker::startHeartbeat() {
    qDebug() << Q_FUNC_INFO << "start heartbeat...";
    // 此时这个函数运行在子线程中
    // QTimer必须在其所属的线程中创建和start
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &HeartbeatWorker::heartbeatTriggered);

    // 间隔60000毫秒 = 60秒
    m_timer->start(60000);

    // 也可以选择立即触发一次，根据需求而定
    // emit heartbeatTriggered();
}

