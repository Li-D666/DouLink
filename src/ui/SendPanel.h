#pragma once

#include <QByteArray>
#include <QFrame>

class QTimer;

namespace Ui {
class SendPanel;
}

namespace doulink {
namespace ui {

// Send composer: text / hex input, line ending suffix and optional periodic
// send. MainWindow routes the payload to all checked sessions.
// Layout lives in sendpanel.ui; this class only owns behavior.
class SendPanel : public QFrame
{
    Q_OBJECT

public:
    explicit SendPanel(QWidget* parent = nullptr);
    ~SendPanel() override;

    void setCanSend(int onlineTargetCount);
    bool isAutoSendActive() const;
    void stopAutoSend();

signals:
    void sendRequested(const QByteArray& payload, const QByteArray& suffix);

private slots:
    void updateMeta();
    void onFormatChanged();
    void onSuffixChanged(int index);
    void onAutoSendToggled(bool enabled);
    void doSend();

private:
    bool buildPayload(QByteArray* payload,
                      QByteArray* suffix,
                      QString* error,
                      bool* empty) const;
    void fireTimer();

    ::Ui::SendPanel* ui = nullptr;
    QTimer* m_timer = nullptr;
    int m_targetCount = 0;
};

} // namespace ui
} // namespace doulink
