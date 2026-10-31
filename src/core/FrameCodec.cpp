#include "core/FrameCodec.h"

#include <QtGlobal>

namespace doulink {

FrameCodec::FrameCodec(FrameMode mode, const QByteArray& delimiter, int maxFrameSize)
    : m_mode(mode)
    , m_delimiter(delimiter)
    , m_maxFrameSize(qMax(64, maxFrameSize))
{
}

QVector<QByteArray> FrameCodec::feed(const QByteArray& chunk)
{
    QVector<QByteArray> frames;
    if (chunk.isEmpty())
        return frames;

    switch (m_mode) {
    case FrameMode::Raw:
        frames.append(chunk);
        return frames;

    case FrameMode::Line: {
        if (m_delimiter.isEmpty()) {
            frames.append(chunk);
            return frames;
        }

        m_buffer.append(chunk);

        int consumed = 0;
        for (;;) {
            const int index = m_buffer.indexOf(m_delimiter, consumed);
            if (index < 0)
                break;

            const int end = index + m_delimiter.size();
            frames.append(m_buffer.mid(consumed, end - consumed));
            consumed = end;
        }

        if (consumed > 0)
            m_buffer.remove(0, consumed);

        // A stream without a delimiter must not grow without bound.
        if (m_buffer.size() > m_maxFrameSize) {
            frames.append(m_buffer);
            m_buffer.clear();
        }
        return frames;
    }

    case FrameMode::IdleGap:
        m_buffer.append(chunk);
        if (m_buffer.size() >= m_maxFrameSize) {
            frames.append(m_buffer);
            m_buffer.clear();
        }
        return frames;
    }

    return frames;
}

QVector<QByteArray> FrameCodec::flush()
{
    QVector<QByteArray> frames;
    if (!m_buffer.isEmpty()) {
        frames.append(m_buffer);
        m_buffer.clear();
    }
    return frames;
}

void FrameCodec::reset()
{
    m_buffer.clear();
}

} // namespace doulink
