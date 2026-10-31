#include "app/Session.h"

#include <QDateTime>

#include <utility>

namespace doulink {

Session::Session(SessionId id,
                 const EndpointConfig& config,
                 std::unique_ptr<ITransport> transport,
                 QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_config(config)
    , m_name(config.name.trimmed().isEmpty() ? defaultSessionName(config) : config.name.trimmed())
    , m_transport(std::move(transport))
{
    if (m_transport) {
        connect(m_transport.get(), &ITransport::opened, this, &Session::onOpened);
        connect(m_transport.get(), &ITransport::closed, this, &Session::onClosed);
        connect(m_transport.get(), &ITransport::errorOccurred, this, &Session::onTransportError);
        connect(m_transport.get(), &ITransport::dataReceived, this, &Session::onDataReceived);
        connect(m_transport.get(), &ITransport::peerChanged, this, &Session::onPeerChanged);
    }

    m_rateTimer = new QTimer(this);
    m_rateTimer->setInterval(1000);
    connect(m_rateTimer, &QTimer::timeout, this, &Session::refreshRates);
    m_rateTimer->start();
    m_rateClock.start();
}

Session::~Session() = default;

void Session::setName(const QString& name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        trimmed = defaultSessionName(m_config);
    if (trimmed == m_name)
        return;
    m_name = trimmed;
    emit nameChanged(this);
}

QString Session::stateText() const
{
    switch (m_state) {
    case SessionState::Disconnected:
        return QStringLiteral("未连接");
    case SessionState::Connecting:
        return QStringLiteral("连接中");
    case SessionState::Connected:
        if (m_config.kind == TransportKind::Serial)
            return QStringLiteral("已打开");
        if (m_config.kind == TransportKind::Udp)
            return QStringLiteral("已就绪");
        return QStringLiteral("已连接");
    case SessionState::Listening:
        return QStringLiteral("监听中");
    case SessionState::Error:
        return QStringLiteral("异常");
    }
    return sessionStateText(m_state);
}

QString Session::endpointText() const
{
    if (m_transport
        && (m_state == SessionState::Connecting || m_state == SessionState::Connected
            || m_state == SessionState::Listening)) {
        const QString description = m_transport->describe();
        if (!description.isEmpty())
            return description;
    }
    return endpointDescription(m_config);
}

void Session::open()
{
    if (m_state == SessionState::Connecting || isOnline())
        return;
    if (!m_transport) {
        appendError(QStringLiteral("无法连接：传输层未创建"));
        setState(SessionState::Error);
        return;
    }

    setState(SessionState::Connecting);    switch (m_config.kind) {
    case TransportKind::Serial:
        appendSystem(QStringLiteral("正在打开串口 %1 ...").arg(endpointDescription(m_config)));
        break;
    case TransportKind::TcpServer:
        appendSystem(QStringLiteral("开始监听 %1 ...").arg(endpointDescription(m_config)));
        break;
    case TransportKind::TcpClient:
        appendSystem(QStringLiteral("正在连接 %1 ...").arg(endpointDescription(m_config)));
        break;
    case TransportKind::Udp:
        appendSystem(QStringLiteral("正在启动 UDP %1 ...").arg(endpointDescription(m_config)));
        break;
    }

    m_transport->open(m_config);
}

void Session::close()
{
    if (!m_transport || m_state == SessionState::Disconnected)
        return;

    appendSystem(QStringLiteral("正在关闭连接 ..."));
    m_transport->close();

    // A transport in error state may not emit closed(); normalise explicitly.
    if (m_state != SessionState::Disconnected) {
        flushAllPending();
        m_peerSet.clear();
        m_stats.peers = 0;
        setState(SessionState::Disconnected);
        appendSystem(QStringLiteral("连接已关闭"));
        emit statsChanged(this);
    }
}

bool Session::send(const QByteArray& payload, const QByteArray& suffix)
{
    if (!isOnline()) {
        appendError(QStringLiteral("发送失败：当前连接未建立"));
        return false;
    }
    if (!m_transport) {
        appendError(QStringLiteral("发送失败：传输层未创建"));
        return false;
    }

    QByteArray frame = payload;
    frame.append(suffix);
    if (frame.isEmpty()) {
        appendError(QStringLiteral("发送失败：内容为空"));
        return false;
    }

    const qint64 written = m_transport->write(frame);
    if (written <= 0) {
        const QString reason = m_transport->lastError().trimmed();
        appendError(QStringLiteral("发送失败：%1")
                        .arg(reason.isEmpty() ? QStringLiteral("传输层拒绝写入") : reason));
        return false;
    }

    m_stats.txBytes += static_cast<quint64>(written);
    ++m_stats.txFrames;
    appendLog(Direction::Tx, frame, txPeerLabel());
    emit statsChanged(this);
    return true;
}

void Session::clearHistory()
{
    m_history.clear();
    emit historyCleared(this);
}

void Session::resetStatistics()
{
    m_stats.rxBytes = 0;
    m_stats.txBytes = 0;
    m_stats.rxFrames = 0;
    m_stats.txFrames = 0;
    m_stats.rxRate = 0.0;
    m_stats.txRate = 0.0;
    m_lastRxBytes = 0;
    m_lastTxBytes = 0;
    m_rateClock.restart();
    emit statsChanged(this);
}

void Session::onOpened()
{
    switch (m_config.kind) {
    case TransportKind::Serial:
        setState(SessionState::Connected);
        m_stats.peers = 1;
        appendSystem(QStringLiteral("串口已打开：%1").arg(m_transport->describe()));
        break;
    case TransportKind::TcpClient:
        setState(SessionState::Connected);
        m_stats.peers = 1;
        appendSystem(QStringLiteral("TCP 已连接：%1").arg(m_transport->describe()));
        break;
    case TransportKind::TcpServer:
        setState(SessionState::Listening);
        m_stats.peers = m_peerSet.size();
        appendSystem(QStringLiteral("监听成功：%1").arg(m_transport->describe()));
        break;
    case TransportKind::Udp:
        setState(SessionState::Connected);
        m_stats.peers = m_peerSet.size();
        appendSystem(QStringLiteral("UDP 已就绪：%1").arg(m_transport->describe()));
        break;
    }
    emit statsChanged(this);
}

void Session::onClosed()
{
    if (m_state == SessionState::Disconnected)
        return;

    flushAllPending();
    m_peerSet.clear();
    m_stats.peers = 0;

    if (m_state == SessionState::Error) {
        // Keep the error state visible until the user retries or closes.
        appendSystem(QStringLiteral("链路已关闭"));
    } else {
        setState(SessionState::Disconnected);
        appendSystem(QStringLiteral("连接已断开"));
    }
    emit statsChanged(this);
}

void Session::onTransportError(const QString& message)
{
    appendError(message);
    setState(SessionState::Error);
}

void Session::onDataReceived(const QByteArray& data, const QString& peer)
{
    if (data.isEmpty())
        return;

    m_stats.rxBytes += static_cast<quint64>(data.size());

    if ((m_config.kind == TransportKind::TcpServer || m_config.kind == TransportKind::Udp)
        && !peer.isEmpty()) {
        m_peerSet.insert(peer);
        m_stats.peers = m_peerSet.size();
    }

    auto it = m_codecs.find(peer);
    if (it == m_codecs.end()) {
        it = m_codecs.insert(
            peer, FrameCodec(m_config.frameMode, m_config.delimiter, m_config.maxFrameSize));
    }

    const QVector<QByteArray> frames = it.value().feed(data);
    processFrames(peer, frames);

    if (m_config.frameMode == FrameMode::IdleGap)
        idleTimerFor(peer)->start(qMax(1, m_config.idleGapMs));
}

void Session::onPeerChanged(const QString& peer, bool connected)
{
    if (peer.isEmpty())
        return;

    if (connected) {
        m_peerSet.insert(peer);
        appendSystem(QStringLiteral("客户端接入：%1（当前 %2 个）")
                         .arg(peer)
                         .arg(m_peerSet.size()));
    } else {
        auto codecIt = m_codecs.find(peer);
        if (codecIt != m_codecs.end()) {
            const QVector<QByteArray> frames = codecIt.value().flush();
            m_codecs.erase(codecIt);
            processFrames(peer, frames);
        }
        if (QTimer* timer = m_idleTimers.take(peer)) {
            timer->stop();
            timer->deleteLater();
        }
        if (m_peerSet.remove(peer)) {
            appendSystem(QStringLiteral("客户端断开：%1（当前 %2 个）")
                             .arg(peer)
                             .arg(m_peerSet.size()));
        }
    }

    m_stats.peers = m_peerSet.size();
    emit statsChanged(this);
}

void Session::refreshRates()
{
    const qint64 elapsedMs = m_rateClock.restart();
    if (elapsedMs <= 0)
        return;

    const quint64 rxDelta = m_stats.rxBytes - m_lastRxBytes;
    const quint64 txDelta = m_stats.txBytes - m_lastTxBytes;
    const double rxRate = static_cast<double>(rxDelta) * 1000.0 / static_cast<double>(elapsedMs);
    const double txRate = static_cast<double>(txDelta) * 1000.0 / static_cast<double>(elapsedMs);

    const bool changed = rxDelta != 0 || txDelta != 0
        || !qFuzzyCompare(m_stats.rxRate + 1.0, rxRate + 1.0)
        || !qFuzzyCompare(m_stats.txRate + 1.0, txRate + 1.0);

    m_lastRxBytes = m_stats.rxBytes;
    m_lastTxBytes = m_stats.txBytes;
    m_stats.rxRate = rxRate;
    m_stats.txRate = txRate;

    if (changed)
        emit statsChanged(this);
}

void Session::setState(SessionState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(this);
}

QTimer* Session::idleTimerFor(const QString& peer)
{
    if (QTimer* existing = m_idleTimers.value(peer))
        return existing;

    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, [this, peer]() {
        auto it = m_codecs.find(peer);
        if (it == m_codecs.end())
            return;
        processFrames(peer, it.value().flush());
    });
    m_idleTimers.insert(peer, timer);
    return timer;
}

void Session::processFrames(const QString& peer, const QVector<QByteArray>& frames)
{
    if (frames.isEmpty())
        return;

    for (const QByteArray& frame : frames) {
        if (frame.isEmpty())
            continue;
        ++m_stats.rxFrames;
        appendLog(Direction::Rx, frame, peer);
    }
}

void Session::flushAllPending()
{
    for (auto* timer : qAsConst(m_idleTimers))
        timer->stop();

    if (!m_codecs.isEmpty()) {
        for (auto it = m_codecs.begin(); it != m_codecs.end(); ++it) {
            const QVector<QByteArray> frames = it.value().flush();
            for (const QByteArray& frame : frames) {
                if (frame.isEmpty())
                    continue;
                ++m_stats.rxFrames;
                appendLog(Direction::Rx, frame, it.key());
            }
        }
        m_codecs.clear();
    }
}

void Session::appendLog(Direction direction,
                        const QByteArray& payload,
                        const QString& peer,
                        const QString& message)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.direction = direction;
    entry.peer = peer;
    entry.payload = payload;
    entry.message = message;
    m_history.append(entry);

    if (m_history.size() > kHistorySoftLimit)
        m_history.remove(0, m_history.size() - kHistoryHardLimit);

    emit logAppended(this, entry);
}

void Session::appendSystem(const QString& message)
{
    appendLog(Direction::System, QByteArray(), QString(), message);
}

void Session::appendError(const QString& message)
{
    appendLog(Direction::Error, QByteArray(), QString(), message);
}

QString Session::txPeerLabel() const
{
    if (m_config.kind == TransportKind::TcpServer) {
        if (m_stats.peers <= 0)
            return QStringLiteral("无客户端");
        return QStringLiteral("%1 个客户端").arg(m_stats.peers);
    }
    if (m_config.kind == TransportKind::Udp) {
        const QString host = m_config.host.trimmed();
        if (!host.isEmpty())
            return QStringLiteral("%1:%2").arg(host).arg(m_config.remotePort);
    }
    return {};
}

} // namespace doulink
