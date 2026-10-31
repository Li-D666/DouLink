#include "infra/TcpServerTransport.h"

namespace doulink {
namespace infra {

TcpServerTransport::TcpServerTransport(QObject* parent)
    : ITransport(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &TcpServerTransport::onNewConnection);
}

TcpServerTransport::~TcpServerTransport()
{
    m_listening = false;
    const auto sockets = m_peerBySocket.keys();
    for (QTcpSocket* socket : sockets) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    m_peerBySocket.clear();
    m_socketByPeer.clear();
    m_server->close();
}

void TcpServerTransport::open(const EndpointConfig& config)
{
    close();

    m_config = config;
    m_lastError.clear();

    QHostAddress address;
    const QString bind = config.bindAddress.trimmed();
    if (bind.isEmpty()) {
        address = QHostAddress(QHostAddress::AnyIPv4);
    } else if (!address.setAddress(bind)) {
        m_lastError = QStringLiteral("监听地址无效：%1").arg(bind);
        emit errorOccurred(m_lastError);
        return;
    }

    if (!m_server->listen(address, config.localPort)) {
        m_lastError = QStringLiteral("监听失败：%1").arg(m_server->errorString());
        emit errorOccurred(m_lastError);
        return;
    }

    m_listening = true;
    emit opened();
}

void TcpServerTransport::close()
{
    const bool wasListening = m_listening;
    m_listening = false;
    m_server->close();

    const auto peers = m_peerBySocket.values();
    for (const QString& peer : peers) {
        QTcpSocket* socket = m_socketByPeer.value(peer);
        if (!socket)
            continue;
        QObject::disconnect(socket, nullptr, this, nullptr);
        socket->abort();
        socket->deleteLater();
    }
    m_peerBySocket.clear();
    m_socketByPeer.clear();

    for (const QString& peer : peers)
        emit peerChanged(peer, false);

    if (wasListening)
        emit closed();
}

bool TcpServerTransport::isOpen() const
{
    return m_listening && m_server->isListening();
}

qint64 TcpServerTransport::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("服务端未监听");
        return -1;
    }

    if (m_peerBySocket.isEmpty()) {
        m_lastError = QStringLiteral("没有已连接的客户端");
        return 0;
    }

    qint64 total = 0;
    const auto sockets = m_peerBySocket.keys();
    for (QTcpSocket* socket : sockets) {
        if (!socket || !m_peerBySocket.contains(socket)
            || socket->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        const qint64 written = socket->write(data);
        if (written > 0)
            total += written;
    }

    if (total <= 0)
        m_lastError = QStringLiteral("写入失败，客户端连接可能已断开");
    return total;
}

QString TcpServerTransport::describe() const
{
    QString address;
    quint16 port = m_config.localPort;
    if (m_listening) {
        address = m_server->serverAddress().toString();
        port = m_server->serverPort();
    } else {
        address = m_config.bindAddress.trimmed().isEmpty()
            ? QStringLiteral("0.0.0.0")
            : m_config.bindAddress.trimmed();
    }
    if (address.contains(QLatin1Char(':')) && !address.startsWith(QLatin1Char('[')))
        address = QStringLiteral("[%1]").arg(address);

    return QStringLiteral("监听 %1:%2 · %3 个客户端")
        .arg(address)
        .arg(port)
        .arg(m_peerBySocket.size());
}

void TcpServerTransport::onNewConnection()
{
    while (QTcpSocket* socket = m_server->nextPendingConnection()) {
        const QString base = QStringLiteral("%1:%2")
                                 .arg(socket->peerAddress().toString())
                                 .arg(socket->peerPort());
        QString peer = base;
        int duplicate = 2;
        while (m_socketByPeer.contains(peer))
            peer = QStringLiteral("%1#%2").arg(base).arg(duplicate++);

        m_peerBySocket.insert(socket, peer);
        m_socketByPeer.insert(peer, socket);
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

        connect(socket, &QTcpSocket::readyRead, this, [this, socket, peer]() {
            const QByteArray data = socket->readAll();
            if (!data.isEmpty())
                emit dataReceived(data, peer);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            removePeer(socket);
        });

        emit peerChanged(peer, true);

        const QByteArray pending = socket->readAll();
        if (!pending.isEmpty())
            emit dataReceived(pending, peer);
    }
}

void TcpServerTransport::removePeer(QTcpSocket* socket)
{
    auto it = m_peerBySocket.find(socket);
    if (it == m_peerBySocket.end())
        return;

    const QString peer = it.value();
    const QByteArray remaining = socket->readAll();
    if (!remaining.isEmpty())
        emit dataReceived(remaining, peer);

    m_peerBySocket.erase(it);
    m_socketByPeer.remove(peer);

    QObject::disconnect(socket, nullptr, this, nullptr);
    socket->deleteLater();

    emit peerChanged(peer, false);
}

} // namespace infra
} // namespace doulink
