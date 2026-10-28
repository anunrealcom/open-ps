#pragma once

#include <QObject>
#include <QJsonObject>
#include <QTcpServer>

class QTcpSocket;

// ============================================================================
// AiController: 由 MainWindow 实现。AiServer 在主线程同步调用它执行指令，
// 保证与 UI 同一线程、可撤销、线程安全。
// ============================================================================
class AiController
{
public:
    virtual ~AiController() = default;
    virtual QJsonObject handleCommand(const QJsonObject &request) = 0;
};

// ============================================================================
// AiServer: 本地 HTTP + JSON 指令服务（仅监听 127.0.0.1）。
// POST /command  body=JSON -> 调用 controller->handleCommand -> 返回 JSON。
// ============================================================================
class AiServer : public QObject
{
    Q_OBJECT
public:
    explicit AiServer(AiController *controller, QObject *parent = nullptr);

    bool start(quint16 port = 48123);
    void stop();
    bool isListening() const { return m_server.isListening(); }
    quint16 port() const { return m_port; }

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    void handleRequest(QTcpSocket *sock, const QByteArray &request);
    void respond(QTcpSocket *sock, int code, const QByteArray &jsonBody);

    AiController *m_ctrl = nullptr;
    QTcpServer m_server;
    quint16 m_port = 48123;
};
