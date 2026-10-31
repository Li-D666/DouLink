#include "core/Types.h"

namespace doulink {
namespace {

QString socketAddress(const QString& host, quint16 port)
{
    const QString trimmed = host.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("*:%1").arg(port);

    if (trimmed.contains(QLatin1Char(':')) && !trimmed.startsWith(QLatin1Char('[')))
        return QStringLiteral("[%1]:%2").arg(trimmed).arg(port);
    return QStringLiteral("%1:%2").arg(trimmed).arg(port);
}

QString parityLetter(Parity parity)
{
    switch (parity) {
    case Parity::None:
        return QStringLiteral("N");
    case Parity::Even:
        return QStringLiteral("E");
    case Parity::Odd:
        return QStringLiteral("O");
    case Parity::Space:
        return QStringLiteral("S");
    case Parity::Mark:
        return QStringLiteral("M");
    }
    return QStringLiteral("N");
}

QString stopBitsText(StopBits stopBits)
{
    switch (stopBits) {
    case StopBits::One:
        return QStringLiteral("1");
    case StopBits::OneAndHalf:
        return QStringLiteral("1.5");
    case StopBits::Two:
        return QStringLiteral("2");
    }
    return QStringLiteral("1");
}

QString flowControlText(FlowControl flowControl)
{
    switch (flowControl) {
    case FlowControl::None:
        return QStringLiteral("无流控");
    case FlowControl::Hardware:
        return QStringLiteral("硬件流控");
    case FlowControl::Software:
        return QStringLiteral("软件流控");
    }
    return QStringLiteral("无流控");
}

} // namespace

QString transportKindName(TransportKind kind)
{
    switch (kind) {
    case TransportKind::Serial:
        return QStringLiteral("串口");
    case TransportKind::TcpClient:
        return QStringLiteral("TCP 客户端");
    case TransportKind::TcpServer:
        return QStringLiteral("TCP 服务端");
    case TransportKind::Udp:
        return QStringLiteral("UDP");
    }
    return QStringLiteral("未知");
}

QString transportKindShortName(TransportKind kind)
{
    switch (kind) {
    case TransportKind::Serial:
        return QStringLiteral("SERIAL");
    case TransportKind::TcpClient:
    case TransportKind::TcpServer:
        return QStringLiteral("TCP");
    case TransportKind::Udp:
        return QStringLiteral("UDP");
    }
    return QStringLiteral("?");
}

QString sessionStateText(SessionState state)
{
    switch (state) {
    case SessionState::Disconnected:
        return QStringLiteral("未连接");
    case SessionState::Connecting:
        return QStringLiteral("连接中");
    case SessionState::Connected:
        return QStringLiteral("已连接");
    case SessionState::Listening:
        return QStringLiteral("监听中");
    case SessionState::Error:
        return QStringLiteral("异常");
    }
    return QStringLiteral("未知");
}

QString serialSettingsText(const EndpointConfig& config)
{
    return QStringLiteral("%1 · %2%3%4 · %5")
        .arg(config.baudRate)
        .arg(static_cast<int>(config.dataBits))
        .arg(parityLetter(config.parity))
        .arg(stopBitsText(config.stopBits))
        .arg(flowControlText(config.flowControl));
}

QString defaultSessionName(const EndpointConfig& config)
{
    switch (config.kind) {
    case TransportKind::Serial: {
        const QString port = config.portName.trimmed().isEmpty()
            ? QStringLiteral("未选择")
            : config.portName.trimmed();
        return QStringLiteral("串口 %1 @%2").arg(port).arg(config.baudRate);
    }
    case TransportKind::TcpClient:
        return QStringLiteral("TCP 客户端 %1").arg(socketAddress(config.host, config.remotePort));
    case TransportKind::TcpServer:
        return QStringLiteral("TCP 服务端 :%1").arg(config.localPort);
    case TransportKind::Udp: {
        if (config.localPort != 0)
            return QStringLiteral("UDP :%1").arg(config.localPort);
        if (!config.host.trimmed().isEmpty())
            return QStringLiteral("UDP %1").arg(socketAddress(config.host, config.remotePort));
        return QStringLiteral("UDP 监听");
    }
    }
    return QStringLiteral("会话");
}

QString endpointDescription(const EndpointConfig& config)
{
    switch (config.kind) {
    case TransportKind::Serial:
        return QStringLiteral("%1 · %2")
            .arg(config.portName.trimmed().isEmpty() ? QStringLiteral("未选择串口") : config.portName.trimmed(),
                 serialSettingsText(config));
    case TransportKind::TcpClient:
        return QStringLiteral("%1 · 连接超时 %2 ms")
            .arg(socketAddress(config.host, config.remotePort))
            .arg(config.connectTimeoutMs);
    case TransportKind::TcpServer: {
        const QString bind = config.bindAddress.trimmed().isEmpty()
            ? QStringLiteral("0.0.0.0")
            : config.bindAddress.trimmed();
        return QStringLiteral("监听 %1 · 支持多客户端")
            .arg(socketAddress(bind, config.localPort));
    }
    case TransportKind::Udp: {
        const QString local = config.localPort == 0
            ? QStringLiteral("自动分配")
            : QString::number(config.localPort);
        const QString remote = config.host.trimmed().isEmpty()
            ? QStringLiteral("仅接收")
            : socketAddress(config.host, config.remotePort);
        return QStringLiteral("本机 :%1 → %2").arg(local, remote);
    }
    }
    return {};
}

} // namespace doulink
