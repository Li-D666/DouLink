#include "infra/SerialTransport.h"

namespace doulink {
namespace infra {
namespace {

QSerialPort::DataBits mapDataBits(DataBits bits)
{
    switch (bits) {
    case DataBits::Five:
        return QSerialPort::Data5;
    case DataBits::Six:
        return QSerialPort::Data6;
    case DataBits::Seven:
        return QSerialPort::Data7;
    case DataBits::Eight:
        return QSerialPort::Data8;
    }
    return QSerialPort::Data8;
}

QSerialPort::Parity mapParity(Parity parity)
{
    switch (parity) {
    case Parity::None:
        return QSerialPort::NoParity;
    case Parity::Even:
        return QSerialPort::EvenParity;
    case Parity::Odd:
        return QSerialPort::OddParity;
    case Parity::Space:
        return QSerialPort::SpaceParity;
    case Parity::Mark:
        return QSerialPort::MarkParity;
    }
    return QSerialPort::NoParity;
}

QSerialPort::StopBits mapStopBits(StopBits stopBits)
{
    switch (stopBits) {
    case StopBits::One:
        return QSerialPort::OneStop;
    case StopBits::OneAndHalf:
        return QSerialPort::OneAndHalfStop;
    case StopBits::Two:
        return QSerialPort::TwoStop;
    }
    return QSerialPort::OneStop;
}

QSerialPort::FlowControl mapFlowControl(FlowControl flowControl)
{
    switch (flowControl) {
    case FlowControl::None:
        return QSerialPort::NoFlowControl;
    case FlowControl::Hardware:
        return QSerialPort::HardwareControl;
    case FlowControl::Software:
        return QSerialPort::SoftwareControl;
    }
    return QSerialPort::NoFlowControl;
}

} // namespace

SerialTransport::SerialTransport(QObject* parent)
    : ITransport(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialTransport::onReadyRead);
    connect(&m_port,
            &QSerialPort::errorOccurred,
            this,
            &SerialTransport::onSerialError);
}

SerialTransport::~SerialTransport()
{
    m_open = false;
    if (m_port.isOpen())
        m_port.close();
}

void SerialTransport::open(const EndpointConfig& config)
{
    if (m_port.isOpen())
        close();

    m_config = config;
    m_lastError.clear();

    if (config.portName.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("未选择串口");
        emit errorOccurred(m_lastError);
        return;
    }

    m_port.setPortName(config.portName.trimmed());
    m_port.setBaudRate(config.baudRate);
    m_port.setDataBits(mapDataBits(config.dataBits));
    m_port.setParity(mapParity(config.parity));
    m_port.setStopBits(mapStopBits(config.stopBits));
    m_port.setFlowControl(mapFlowControl(config.flowControl));

    if (!m_port.open(QIODevice::ReadWrite)) {
        m_lastError = QStringLiteral("串口打开失败：%1").arg(m_port.errorString());
        emit errorOccurred(m_lastError);
        return;
    }

    m_open = true;
    emit opened();
}

void SerialTransport::close()
{
    const bool wasActive = m_open || m_port.isOpen();
    m_open = false;
    if (m_port.isOpen()) {
        const QByteArray pending = m_port.readAll();
        if (!pending.isEmpty())
            emit dataReceived(pending, QString());
        m_port.close();
    }
    if (wasActive)
        emit closed();
}

bool SerialTransport::isOpen() const
{
    return m_open && m_port.isOpen();
}

qint64 SerialTransport::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("串口未打开");
        return -1;
    }

    const qint64 written = m_port.write(data);
    if (written < 0)
        m_lastError = m_port.errorString();
    return written;
}

QString SerialTransport::describe() const
{
    const QString port = m_config.portName.trimmed().isEmpty()
        ? QStringLiteral("未选择串口")
        : m_config.portName.trimmed();
    return QStringLiteral("%1 · %2").arg(port, serialSettingsText(m_config));
}

void SerialTransport::onReadyRead()
{
    const QByteArray data = m_port.readAll();
    if (!data.isEmpty())
        emit dataReceived(data, QString());
}

void SerialTransport::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    // Errors raised while QSerialPort::open() is failing are reported by open().
    if (!m_open)
        return;

    const bool fatal = error == QSerialPort::ResourceError
        || error == QSerialPort::PermissionError
        || error == QSerialPort::DeviceNotFoundError
        || error == QSerialPort::OpenError;

    if (!fatal) {
        // Parity/framing/read errors can be transient on a noisy RS-485 bus.
        // Keep the port open and let subsequent frames decide.
        m_lastError = m_port.errorString();
        return;
    }

    m_lastError = QStringLiteral("串口异常：%1").arg(m_port.errorString());
    m_open = false;
    if (m_port.isOpen())
        m_port.close();

    emit errorOccurred(m_lastError);
    emit closed();
}

} // namespace infra
} // namespace doulink
