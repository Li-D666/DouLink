#include "infra/UdpTransport.h"

#include <QHostInfo>

namespace doulink {
namespace infra {

UdpTransport::UdpTransport(QObject* parent)
    : ITransport(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpTransport::onReadyRead);
}

UdpTransport::~UdpTransport()
{
    m_open = false;
    m_socket->close();
}

void UdpTransport::open(const EndpointConfig& config)
{
    close();

    m_config = config;
    m_lastError.clear();

    QHostAddress bindAddress;
    const QString bind = config.bindAddress.trimmed();
    if (bind.isEmpty()) {
        bindAddress = QHostAddress(QHostAddress::AnyIPv4);
    } else if (!bindAddress.setAddress(bind)) {
        m_lastError = QStringLiteral("本机绑定地址无效：%1").arg(bind);
        emit errorOccurred(m_lastError);
        return;
    }

    if (!m_socket->bind(bindAddress, config.localPort)) {
        m_lastError = QStringLiteral("UDP 绑定失败：%1").arg(m_socket->errorString());
        emit errorOccurred(m_lastError);
        return;
    }

    m_remoteAddress = QHostAddress();
    const QString host = config.host.trimmed();
    if (!host.isEmpty()) {
        QHostAddress direct;
        if (direct.setAddress(host)) {
            m_remoteAddress = direct;
        } else {
            // Host names are resolved once at open time; the UI stays responsive
            // for IP literals, which are the normal case in device debugging.
            const QHostInfo info = QHostInfo::fromName(host);
            if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
                m_socket->close();
                m_lastError = QStringLiteral("目标地址解析失败：%1").arg(host);
                emit errorOccurred(m_lastError);
                return;
            }

            // Prefer IPv4 so broadcast / legacy devices behave predictably.
            for (const QHostAddress& address : info.addresses()) {
                if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                    m_remoteAddress = address;
                    break;
                }
            }
            if (m_remoteAddress.isNull())
                m_remoteAddress = info.addresses().first();
        }
    }

    m_open = true;
    emit opened();
}

void UdpTransport::close()
{
    const bool wasOpen = m_open;
    m_open = false;
    m_socket->close();
    m_remoteAddress.clear();
    if (wasOpen)
        emit closed();
}

bool UdpTransport::isOpen() const
{
    return m_open && m_socket->state() != QAbstractSocket::UnconnectedState;
}

qint64 UdpTransport::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("UDP 未就绪");
        return -1;
    }
    if (m_remoteAddress.isNull() || m_config.remotePort == 0) {
        m_lastError = QStringLiteral("未设置目标地址和端口");
        return -1;
    }
    if (data.size() > 65507) {
        m_lastError = QStringLiteral("UDP 单包不能超过 65507 字节");
        return -1;
    }

    const qint64 written = m_socket->writeDatagram(data, m_remoteAddress, m_config.remotePort);
    if (written < 0)
        m_lastError = QStringLiteral("UDP 发送失败：%1").arg(m_socket->errorString());
    return written;
}

QString UdpTransport::describe() const
{
    QString local;
    if (m_open) {
        local = QStringLiteral("%1:%2")
                    .arg(m_socket->localAddress().toString())
                    .arg(m_socket->localPort());
    } else if (m_config.localPort == 0) {
        local = QStringLiteral("自动分配");
    } else {
        local = QStringLiteral(":%1").arg(m_config.localPort);
    }

    const QString remote = m_config.host.trimmed().isEmpty()
        ? QStringLiteral("仅接收")
        : QStringLiteral("%1:%2").arg(m_config.host.trimmed()).arg(m_config.remotePort);

    return QStringLiteral("UDP 本机 %1 → %2").arg(local, remote);
}

void UdpTransport::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        const qint64 pending = m_socket->pendingDatagramSize();
        QByteArray datagram;
        datagram.resize(static_cast<int>(qMax<qint64>(1, pending)));

        QHostAddress sender;
        quint16 senderPort = 0;
        const qint64 read = m_socket->readDatagram(
            datagram.data(), datagram.size(), &sender, &senderPort);
        if (read < 0)
            break;

        datagram.resize(static_cast<int>(read));
        QString peer = QStringLiteral("%1:%2").arg(sender.toString()).arg(senderPort);
        emit dataReceived(datagram, peer);
    }
}

} // namespace infra
} // namespace doulink
