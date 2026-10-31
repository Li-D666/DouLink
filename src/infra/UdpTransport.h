#pragma once

#include "core/ITransport.h"

#include <QHostAddress>
#include <QUdpSocket>

namespace doulink {
namespace infra {

class UdpTransport final : public ITransport
{
    Q_OBJECT

public:
    explicit UdpTransport(QObject* parent = nullptr);
    ~UdpTransport() override;

    void open(const EndpointConfig& config) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray& data) override;
    QString lastError() const override { return m_lastError; }
    QString describe() const override;

private slots:
    void onReadyRead();

private:
    QUdpSocket* m_socket = nullptr;
    QHostAddress m_remoteAddress;
    EndpointConfig m_config;
    bool m_open = false;
    QString m_lastError;
};

} // namespace infra
} // namespace doulink
