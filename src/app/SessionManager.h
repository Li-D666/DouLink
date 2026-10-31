#pragma once

#include "core/ITransport.h"
#include "core/Types.h"

#include <QHash>
#include <QObject>
#include <QVector>

#include <functional>
#include <memory>

namespace doulink {

class Session;

// Application service / composition boundary. The factory callback is injected
// by main.cpp, which keeps this layer free of QtNetwork and QtSerialPort.
class SessionManager final : public QObject
{
    Q_OBJECT

public:
    using TransportFactory = std::function<std::unique_ptr<ITransport>(TransportKind)>;
    using SerialPortLister = std::function<QVector<SerialPortInfo>()>;

    SessionManager(TransportFactory transportFactory,
                   SerialPortLister serialPortLister,
                   QObject* parent = nullptr);
    ~SessionManager() override;

    Session* createSession(const EndpointConfig& config);
    bool removeSession(SessionId id);

    Session* session(SessionId id) const;
    QVector<Session*> sessions() const;

    QVector<SerialPortInfo> serialPorts() const;

    int onlineCount() const;

    // Sends to the selected session ids. Offline/unwritable sessions are
    // skipped. Returns the number of sessions that accepted the payload.
    int sendTo(const QVector<SessionId>& ids,
               const QByteArray& payload,
               const QByteArray& suffix = QByteArray());

    // Sends to every session that currently accepts writes.
    int broadcast(const QByteArray& payload, const QByteArray& suffix = QByteArray());

    void openAll();
    void closeAll();

signals:
    void sessionAdded(Session* session);
    void sessionRemoved(SessionId id);
    void sessionsChanged();

private:
    bool nameExists(const QString& name) const;
    QString uniqueName(const QString& base) const;

    TransportFactory m_factory;
    SerialPortLister m_serialPortLister;
    QHash<SessionId, Session*> m_sessions;
    QVector<SessionId> m_order;
    SessionId m_nextId = 1;
};

} // namespace doulink
