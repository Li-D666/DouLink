#pragma once

#include "core/Types.h"

#include <QByteArray>
#include <QVector>

namespace doulink {

// Incremental receive framing. One FrameCodec belongs to one logical peer.
// It is intentionally free of any QtNetwork / QtSerialPort dependency so it can
// be unit tested and reused by protocol plug-ins (for example a Modbus RTU
// frame splitter implemented by the application itself).
class FrameCodec
{
public:
    FrameCodec() = default;
    FrameCodec(FrameMode mode, const QByteArray& delimiter, int maxFrameSize = 1024 * 1024);

    // Feeds newly received bytes and returns all frames completed by this chunk.
    QVector<QByteArray> feed(const QByteArray& chunk);

    // Emits any buffered bytes (used by idle-gap framing or when closing).
    QVector<QByteArray> flush();

    void reset();
    bool hasBufferedData() const { return !m_buffer.isEmpty(); }

private:
    FrameMode m_mode = FrameMode::Raw;
    QByteArray m_delimiter = QByteArrayLiteral("\n");
    QByteArray m_buffer;
    int m_maxFrameSize = 1024 * 1024;
};

} // namespace doulink
