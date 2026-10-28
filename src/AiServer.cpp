#include "AiServer.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QHostAddress>

AiServer::AiServer(AiController *controller, QObject *parent)
    : QObject(parent), m_ctrl(controller)
{
    connect(&m_server, &QTcpServer::newConnection, this, &AiServer::onNewConnection);
}

bool AiServer::start(quint16 port)
{
    m_port = port;
    return m_server.listen(QHostAddress::LocalHost, port);
}

void AiServer::stop()
{
    m_server.close();
}

void AiServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *sock = m_server.nextPendingConnection();
        sock->setProperty("_buf", QByteArray());
        connect(sock, &QTcpSocket::readyRead, this, &AiServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
}

void AiServer::onReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;

    QByteArray buf = sock->property("_buf").toByteArray();
    buf += sock->readAll();

    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        sock->setProperty("_buf", buf);
        return;   // 头部还没收全
    }
    const QByteArray header = buf.left(headerEnd);
    int contentLength = 0;
    const QList<QByteArray> lines = header.split('\n');
    for (const QByteArray &ln : lines) {
        if (ln.toLower().startsWith("content-length:"))
            contentLength = ln.mid(ln.indexOf(':') + 1).trimmed().toInt();
    }
    const int bodyStart = headerEnd + 4;
    if (buf.size() < bodyStart + contentLength) {
        sock->setProperty("_buf", buf);
        return;   // body 还没收全
    }
    sock->setProperty("_buf", QByteArray());
    handleRequest(sock, buf);
}

void AiServer::handleRequest(QTcpSocket *sock, const QByteArray &request)
{
    const int headerEnd = request.indexOf("\r\n\r\n");
    const QByteArray header = request.left(headerEnd);
    const QByteArray body = request.mid(headerEnd + 4);

    const QByteArray firstLine = header.split('\r').first();   // "POST /command HTTP/1.1"
    const QList<QByteArray> parts = firstLine.split(' ');
    const QByteArray method = parts.value(0);
    const QByteArray path = parts.value(1);

    if (method != "POST") {
        respond(sock, 405, R"({"ok":false,"error":"use POST /command"})");
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        respond(sock, 400, R"({"ok":false,"error":"invalid json"})");
        return;
    }

    QJsonObject result;
    if (m_ctrl)
        result = m_ctrl->handleCommand(doc.object());
    else
        result = QJsonObject{{"ok", false}, {"error", "no controller"}};

    respond(sock, 200, QJsonDocument(result).toJson(QJsonDocument::Compact));
}

void AiServer::respond(QTcpSocket *sock, int code, const QByteArray &jsonBody)
{
    const QByteArray status = (code == 200) ? "OK" : (code == 400 ? "Bad Request"
                              : (code == 405 ? "Method Not Allowed" : "Error"));
    QByteArray resp;
    resp += "HTTP/1.1 " + QByteArray::number(code) + " " + status + "\r\n";
    resp += "Content-Type: application/json; charset=utf-8\r\n";
    resp += "Content-Length: " + QByteArray::number(jsonBody.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += jsonBody;
    sock->write(resp);
    sock->flush();
    sock->disconnectFromHost();
}
