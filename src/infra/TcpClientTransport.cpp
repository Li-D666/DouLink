#include "infra/TcpClientTransport.h"

namespace doulink {
namespace infra {

TcpClientTransport::TcpClientTransport(QObject* parent)
    : ITransport(parent)
    , m_socket(new QTcpSocket(this))
    , m_connectTimer(new QTimer(this))
{
    m_connectTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::connected, this, &TcpClientTransport::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClientTransport::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClientTransport::onReadyRead);
    connect(m_connectTimer, &QTimer::timeout, this, &TcpClientTransport::onConnectTimeout);

    // Qt 5.14 still uses QAbstractSocket::error(SocketError); errorOccurred only
    // appeared in later Qt 5 releases.
    connect(m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this,
            &TcpClientTransport::onSocketError);
}

TcpClientTransport::~TcpClientTransport()
{
    m_open = false;
    m_connecting = false;
    m_connectTimer->stop();
    m_socket->abort();
}

void TcpClientTransport::open(const EndpointConfig& config)
{
    close();

    m_config = config;
    m_lastError.clear();

    if (config.host.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("目标地址为空");
        emit errorOccurred(m_lastError);
        return;
    }
    if (config.remotePort == 0) {
        m_lastError = QStringLiteral("目标端口无效");
        emit errorOccurred(m_lastError);
        return;
    }

    m_open = false;
    m_connecting = true;
    m_closedEmitted = false;
    m_connectTimer->start(qMax(500, config.connectTimeoutMs));
    m_socket->connectToHost(config.host.trimmed(), config.remotePort);
}

void TcpClientTransport::close()
{
    m_connectTimer->stop();

    const bool wasActive = m_open || m_connecting;
    m_open = false;
    m_connecting = false;
    m_closedEmitted = true;
    m_socket->abort();

    if (wasActive)
        emit closed();
}

bool TcpClientTransport::isOpen() const
{
    return m_open && m_socket->state() == QAbstractSocket::ConnectedState;
}

qint64 TcpClientTransport::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("TCP 未连接");
        return -1;
    }

    const qint64 written = m_socket->write(data);
    if (written < 0)
        m_lastError = m_socket->errorString();
    return written;
}

QString TcpClientTransport::describe() const
{
    QString host = m_config.host.trimmed();
    if (host.contains(QLatin1Char(':')) && !host.startsWith(QLatin1Char('[')))
        host = QStringLiteral("[%1]").arg(host);
    return QStringLiteral("%1:%2 · TCP 客户端").arg(host).arg(m_config.remotePort);
}

void TcpClientTransport::onConnected()
{
    if (!m_connecting && !m_open)
        return;

    m_connecting = false;
    m_open = true;
    m_connectTimer->stop();
    m_lastError.clear();

    // Disable Nagle for a request/response debugger; latency matters more than
    // packing tiny frames here.
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    emit opened();
}

void TcpClientTransport::onDisconnected()
{
    if (m_closedEmitted)
        return;

    const QByteArray remaining = m_socket->readAll();
    if (!remaining.isEmpty())
        emit dataReceived(remaining, QString());

    m_connectTimer->stop();
    const bool wasActive = m_open || m_connecting;
    m_open = false;
    m_connecting = false;
    m_closedEmitted = true;

    if (wasActive)
        emit closed();
}

void TcpClientTransport::onReadyRead()
{
    const QByteArray data = m_socket->readAll();
    if (!data.isEmpty())
        emit dataReceived(data, QString());
}

void TcpClientTransport::onSocketError(QAbstractSocket::SocketError error)
{
    if (error == QAbstractSocket::RemoteHostClosedError) {
        // Normal peer shutdown: onDisconnected() reports it.
        return;
    }
    if (m_closedEmitted)
        return;

    m_lastError = QStringLiteral("TCP 错误：%1").arg(m_socket->errorString());
    const bool wasActive = m_open || m_connecting;

    m_connectTimer->stop();
    m_open = false;
    m_connecting = false;
    m_closedEmitted = true;
    m_socket->abort();

    if (wasActive) {
        emit errorOccurred(m_lastError);
        emit closed();
    }
}

void TcpClientTransport::onConnectTimeout()
{
    if (!m_connecting)
        return;

    m_lastError = QStringLiteral("连接超时（%1 ms）").arg(m_config.connectTimeoutMs);
    m_open = false;
    m_connecting = false;
    m_closedEmitted = true;
    m_socket->abort();

    emit errorOccurred(m_lastError);
    emit closed();
}

} // namespace infra
} // namespace doulink
