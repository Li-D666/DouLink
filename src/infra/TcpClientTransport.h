#pragma once

#include "core/ITransport.h"

#include <QAbstractSocket>
#include <QTcpSocket>
#include <QTimer>

namespace doulink {
namespace infra {

class TcpClientTransport final : public ITransport
{
    Q_OBJECT

public:
    explicit TcpClientTransport(QObject* parent = nullptr);
    ~TcpClientTransport() override;

    void open(const EndpointConfig& config) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray& data) override;
    QString lastError() const override { return m_lastError; }
    QString describe() const override;

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onConnectTimeout();

private:
    QTcpSocket* m_socket = nullptr;
    QTimer* m_connectTimer = nullptr;
    EndpointConfig m_config;
    bool m_open = false;
    bool m_connecting = false;
    bool m_closedEmitted = true;
    QString m_lastError;
};

} // namespace infra
} // namespace doulink
