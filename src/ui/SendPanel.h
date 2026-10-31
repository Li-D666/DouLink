#pragma once

#include <QByteArray>
#include <QFrame>

class QTimer;

namespace Ui {
class SendPanel;
}

namespace doulink {
namespace ui {

// Send composer: text / hex input, line ending suffix, optional periodic send
// and a broadcast action that MainWindow routes to all online sessions.
// Layout lives in sendpanel.ui; this class only owns behavior.
class SendPanel : public QFrame
{
    Q_OBJECT

public:
    explicit SendPanel(QWidget* parent = nullptr);
    ~SendPanel() override;

    void setSessionAvailability(bool currentOnline, bool anyOnline);
    bool isAutoSendActive() const;
    void stopAutoSend();

signals:
    void sendRequested(const QByteArray& payload, const QByteArray& suffix);
    void broadcastRequested(const QByteArray& payload, const QByteArray& suffix);

private slots:
    void updateMeta();
    void onFormatChanged();
    void onSuffixChanged(int index);
    void onAutoSendToggled(bool enabled);
    void doSend();
    void doBroadcast();

private:
    bool buildPayload(QByteArray* payload,
                      QByteArray* suffix,
                      QString* error,
                      bool* empty) const;
    void fireTimer();

    ::Ui::SendPanel* ui = nullptr;
    QTimer* m_timer = nullptr;
    bool m_online = false;
    bool m_anyOnline = false;
};

} // namespace ui
} // namespace doulink
