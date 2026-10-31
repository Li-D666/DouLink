#include "app/SessionManager.h"

#include "app/Session.h"

#include <utility>

namespace doulink {

SessionManager::SessionManager(TransportFactory transportFactory,
                               SerialPortLister serialPortLister,
                               QObject* parent)
    : QObject(parent)
    , m_factory(std::move(transportFactory))
    , m_serialPortLister(std::move(serialPortLister))
{
}

SessionManager::~SessionManager() = default;

Session* SessionManager::createSession(const EndpointConfig& config)
{
    if (!m_factory)
        return nullptr;

    std::unique_ptr<ITransport> transport = m_factory(config.kind);
    if (!transport)
        return nullptr;

    const SessionId id = m_nextId++;
    auto* session = new Session(id, config, std::move(transport), this);
    session->setName(uniqueName(session->name()));

    m_sessions.insert(id, session);
    m_order.append(id);

    emit sessionAdded(session);
    emit sessionsChanged();
    return session;
}

bool SessionManager::removeSession(SessionId id)
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return false;

    Session* session = it.value();
    m_sessions.erase(it);
    m_order.removeAll(id);

    session->close();

    emit sessionRemoved(id);
    emit sessionsChanged();

    session->deleteLater();
    return true;
}

Session* SessionManager::session(SessionId id) const
{
    return m_sessions.value(id, nullptr);
}

QVector<Session*> SessionManager::sessions() const
{
    QVector<Session*> result;
    result.reserve(m_order.size());
    for (SessionId id : m_order) {
        if (Session* session = m_sessions.value(id, nullptr))
            result.append(session);
    }
    return result;
}

QVector<SerialPortInfo> SessionManager::serialPorts() const
{
    return m_serialPortLister ? m_serialPortLister() : QVector<SerialPortInfo>();
}

int SessionManager::onlineCount() const
{
    int count = 0;
    for (Session* session : m_sessions) {
        if (session && session->isOnline())
            ++count;
    }
    return count;
}

int SessionManager::sendTo(const QVector<SessionId>& ids,
                           const QByteArray& payload,
                           const QByteArray& suffix)
{
    int delivered = 0;
    for (SessionId id : ids) {
        Session* session = this->session(id);
        if (session && session->isOnline() && session->send(payload, suffix))
            ++delivered;
    }
    return delivered;
}

int SessionManager::broadcast(const QByteArray& payload, const QByteArray& suffix)
{
    int delivered = 0;
    for (Session* session : sessions()) {
        if (session && session->isOnline() && session->send(payload, suffix))
            ++delivered;
    }
    return delivered;
}

void SessionManager::openAll()
{
    for (Session* session : sessions()) {
        if (session && !session->isOnline())
            session->open();
    }
}

void SessionManager::closeAll()
{
    for (Session* session : sessions()) {
        if (session)
            session->close();
    }
}

bool SessionManager::nameExists(const QString& name) const
{
    for (Session* session : m_sessions) {
        if (session && session->name() == name)
            return true;
    }
    return false;
}

QString SessionManager::uniqueName(const QString& base) const
{
    if (!nameExists(base))
        return base;

    int suffix = 2;
    QString candidate;
    do {
        candidate = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
    } while (nameExists(candidate));
    return candidate;
}

} // namespace doulink
