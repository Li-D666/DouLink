#pragma once

#include "core/Types.h"

#include <QDialog>
#include <QVector>

#include <functional>

namespace Ui {
class SessionDialog;
}

namespace doulink {
namespace ui {

// Layout lives in sessiondialog.ui. This class owns validation, defaults and
// conversion between widget indices and EndpointConfig.
class SessionDialog : public QDialog
{
    Q_OBJECT

public:
    using SerialPortProvider = std::function<QVector<SerialPortInfo>()>;

    SessionDialog(QWidget* parent,
                  SerialPortProvider portProvider,
                  TransportKind preset,
                  const EndpointConfig* initial = nullptr);
    ~SessionDialog() override;

    EndpointConfig endpointConfig() const { return m_result; }

protected:
    void accept() override;

private:
    void refreshPorts();
    void applyConfig(const EndpointConfig& config);
    void updateFramingVisibility();
    bool validate(QString* error) const;
    EndpointConfig collectConfig() const;

    SerialPortProvider m_portProvider;
    EndpointConfig m_result;
    ::Ui::SessionDialog* ui = nullptr;
};

} // namespace ui
} // namespace doulink
