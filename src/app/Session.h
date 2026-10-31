#pragma once

#include "core/FrameCodec.h"
#include "core/ITransport.h"
#include "core/Types.h"

#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVector>

#include <memory>

namespace doulink {

// Application-layer aggregate: owns one transport, its receive framing state,
// the bounded log history and statistics. The UI never talks to a transport
// directly; it only observes this object.
class Session : public QObject
{
    Q_OBJECT

public:
    Session(SessionId id,
            const EndpointConfig& config,
            std::unique_ptr<ITransport> transport,
            QObject* parent = nullptr);
    ~Session() override;

    SessionId id() const { return m_id; }
    const EndpointConfig& config() const { return m_config; }

    QString name() const { return m_name; }
    void setName(const QString& name);

    SessionState state() const { return m_state; }
    QString stateText() const;
    QString endpointText() const;
    QString transportText() const { return transportKindName(m_config.kind); }

    bool isOnline() const
    {
        return m_state == SessionState::Connected || m_state == SessionState::Listening;
    }

    SessionStats stats() const { return m_stats; }
    const QVector<LogEntry>& history() const { return m_history; }

    // payload/suffix are appended verbatim and logged as one TX entry.
    bool send(const QByteArray& payload, const QByteArray& suffix = QByteArray());

    void open();
    void close();

    void clearHistory();
    void resetStatistics();

signals:
    void stateChanged(Session* session);
    void logAppended(Session* session, const LogEntry& entry);
    void statsChanged(Session* session);
    void nameChanged(Session* session);
    void historyCleared(Session* session);

private slots:
    void onOpened();
    void onClosed();
    void onTransportError(const QString& message);
    void onDataReceived(const QByteArray& data, const QString& peer);
    void onPeerChanged(const QString& peer, bool connected);
    void refreshRates();

private:
    void setState(SessionState state);
    QTimer* idleTimerFor(const QString& peer);
    void processFrames(const QString& peer, const QVector<QByteArray>& frames);
    void flushAllPending();
    void appendLog(Direction direction,
                   const QByteArray& payload,
                   const QString& peer,
                   const QString& message = QString());
    void appendSystem(const QString& message);
    void appendError(const QString& message);
    QString txPeerLabel() const;

    static constexpr int kHistoryHardLimit = 5000;
    static constexpr int kHistorySoftLimit = 5500;

    SessionId m_id = kInvalidSessionId;
    EndpointConfig m_config;
    QString m_name;
    std::unique_ptr<ITransport> m_transport;
    SessionState m_state = SessionState::Disconnected;
    SessionStats m_stats;
    QVector<LogEntry> m_history;

    QHash<QString, FrameCodec> m_codecs;
    QHash<QString, QTimer*> m_idleTimers;
    QSet<QString> m_peerSet;

    QTimer* m_rateTimer = nullptr;
    QElapsedTimer m_rateClock;
    quint64 m_lastRxBytes = 0;
    quint64 m_lastTxBytes = 0;
};

} // namespace doulink
