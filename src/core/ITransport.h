#pragma once

#include "core/Types.h"

#include <QObject>
#include <QString>

namespace doulink {

// Transport layer contract. Implementations live in doulink::infra and are the
// only place that knows about QSerialPort / QTcpSocket / QUdpSocket.
//
// All methods are expected to be called from the thread that owns the object
// (the GUI thread). I/O itself is asynchronous; no blocking reads are allowed.
class ITransport : public QObject
{
    Q_OBJECT

public:
    explicit ITransport(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~ITransport() override = default;

    virtual void open(const EndpointConfig& config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Returns the number of bytes accepted by the transport, or -1 on failure.
    // A multi-client transport may report the sum for all peers.
    virtual qint64 write(const QByteArray& data) = 0;

    virtual QString lastError() const = 0;

    // Human readable state used by the session header (dynamic, may include the
    // actual listening port or connected peer count).
    virtual QString describe() const = 0;

signals:
    void opened();
    void closed();
    void errorOccurred(const QString& message);

    // peer is empty for point-to-point transports. TCP server and UDP fill it
    // with "address:port" so the application can merge/split peer streams.
    void dataReceived(const QByteArray& data, const QString& peer);

    // Only transports with multiple remote peers emit this.
    void peerChanged(const QString& peer, bool connected);
};

} // namespace doulink
