#pragma once

#include "core/ITransport.h"

#include <QHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

namespace doulink {
namespace infra {

// A session-level listening socket. Every accepted client is tracked as a
// separate peer stream; write() broadcasts to all currently connected clients.
class TcpServerTransport final : public ITransport
{
    Q_OBJECT

public:
    explicit TcpServerTransport(QObject* parent = nullptr);
    ~TcpServerTransport() override;

    void open(const EndpointConfig& config) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray& data) override;
    QString lastError() const override { return m_lastError; }
    QString describe() const override;

private slots:
    void onNewConnection();

private:
    void removePeer(QTcpSocket* socket);

    QTcpServer* m_server = nullptr;
    QHash<QTcpSocket*, QString> m_peerBySocket;
    QHash<QString, QTcpSocket*> m_socketByPeer;
    EndpointConfig m_config;
    bool m_listening = false;
    QString m_lastError;
};

} // namespace infra
} // namespace doulink
