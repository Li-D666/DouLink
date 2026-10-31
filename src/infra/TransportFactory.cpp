#include "infra/TransportFactory.h"

#include "infra/SerialTransport.h"
#include "infra/TcpClientTransport.h"
#include "infra/TcpServerTransport.h"
#include "infra/UdpTransport.h"

#include <QSerialPortInfo>

namespace doulink {
namespace infra {

std::unique_ptr<ITransport> createTransport(TransportKind kind)
{
    switch (kind) {
    case TransportKind::Serial:
        return std::make_unique<SerialTransport>();
    case TransportKind::TcpClient:
        return std::make_unique<TcpClientTransport>();
    case TransportKind::TcpServer:
        return std::make_unique<TcpServerTransport>();
    case TransportKind::Udp:
        return std::make_unique<UdpTransport>();
    }
    return nullptr;
}

QVector<SerialPortInfo> availableSerialPorts()
{
    QVector<SerialPortInfo> result;
    const auto ports = QSerialPortInfo::availablePorts();
    result.reserve(ports.size());
    for (const QSerialPortInfo& port : ports) {
        SerialPortInfo info;
        info.name = port.portName();
        info.description = port.description().trimmed();
        if (info.description.isEmpty())
            info.description = port.manufacturer().trimmed();
        result.append(info);
    }
    return result;
}

} // namespace infra
} // namespace doulink
