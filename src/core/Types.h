#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

namespace doulink {

using SessionId = quint64;
constexpr SessionId kInvalidSessionId = 0;

enum class TransportKind {
    Serial = 0,
    TcpClient,
    TcpServer,
    Udp
};

enum class FrameMode {
    Raw = 0,     // every received chunk is one frame
    Line,        // split on a delimiter (delimiter is kept in the frame)
    IdleGap      // split after a period of bus silence (Modbus-RTU style)
};

enum class DataFormat {
    Text = 0,
    Hex
};

enum class SessionState {
    Disconnected = 0,
    Connecting,
    Connected,
    Listening,
    Error
};

enum class Direction {
    Rx = 0,
    Tx,
    System,
    Error
};

enum class DataBits {
    Five = 5,
    Six = 6,
    Seven = 7,
    Eight = 8
};

enum class Parity {
    None = 0,
    Even,
    Odd,
    Space,
    Mark
};

enum class StopBits {
    One = 1,
    OneAndHalf = 2,
    Two = 3
};

enum class FlowControl {
    None = 0,
    Hardware,
    Software
};

struct SerialPortInfo {
    QString name;
    QString description;

    QString label() const
    {
        return description.isEmpty() ? name : QStringLiteral("%1 · %2").arg(name, description);
    }
};

struct EndpointConfig {
    TransportKind kind = TransportKind::TcpClient;
    QString name; // optional user supplied session name

    // ----- serial -----------------------------------------------------------
    QString portName;
    int baudRate = 115200;
    DataBits dataBits = DataBits::Eight;
    Parity parity = Parity::None;
    StopBits stopBits = StopBits::One;
    FlowControl flowControl = FlowControl::None;

    // ----- tcp client / udp remote -----------------------------------------
    QString host = QStringLiteral("127.0.0.1");
    quint16 remotePort = 8080;
    int connectTimeoutMs = 5000;

    // ----- tcp server / udp local ------------------------------------------
    QString bindAddress; // empty = any IPv4
    quint16 localPort = 0;

    // ----- framing ----------------------------------------------------------
    FrameMode frameMode = FrameMode::Raw;
    QByteArray delimiter = QByteArrayLiteral("\n");
    int idleGapMs = 30;
    int maxFrameSize = 1024 * 1024;
};

struct LogEntry {
    QDateTime timestamp;
    Direction direction = Direction::System;
    QString peer;
    QByteArray payload;
    QString message;
};

struct SessionStats {
    quint64 rxBytes = 0;
    quint64 txBytes = 0;
    quint64 rxFrames = 0;
    quint64 txFrames = 0;
    double rxRate = 0.0; // bytes / second
    double txRate = 0.0; // bytes / second
    int peers = 0;
};

QString transportKindName(TransportKind kind);
QString transportKindShortName(TransportKind kind);
QString sessionStateText(SessionState state);
QString defaultSessionName(const EndpointConfig& config);
QString endpointDescription(const EndpointConfig& config);
QString serialSettingsText(const EndpointConfig& config);

} // namespace doulink

Q_DECLARE_METATYPE(doulink::LogEntry)
Q_DECLARE_METATYPE(doulink::EndpointConfig)
