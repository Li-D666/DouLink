#pragma once

#include "core/ITransport.h"

#include <QSerialPort>

namespace doulink {
namespace infra {

class SerialTransport final : public ITransport
{
    Q_OBJECT

public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    void open(const EndpointConfig& config) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray& data) override;
    QString lastError() const override { return m_lastError; }
    QString describe() const override;

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    QSerialPort m_port;
    EndpointConfig m_config;
    bool m_open = false;
    QString m_lastError;
};

} // namespace infra
} // namespace doulink
