#pragma once

#include "core/ITransport.h"

#include <QVector>

#include <memory>

namespace doulink {
namespace infra {

std::unique_ptr<ITransport> createTransport(TransportKind kind);
QVector<SerialPortInfo> availableSerialPorts();

} // namespace infra
} // namespace doulink
