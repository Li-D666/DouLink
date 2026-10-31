#include "ui/SessionDialog.h"

#include "core/ByteCodec.h"
#include "ui_sessiondialog.h"

#include <QComboBox>
#include <QHostAddress>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QtGlobal>

#include <utility>

namespace doulink {
namespace ui {
namespace {

DataBits dataBitsFromIndex(int index)
{
    return static_cast<DataBits>(5 + qBound(0, index, 3));
}

int indexForDataBits(DataBits bits)
{
    return qBound(0, static_cast<int>(bits) - 5, 3);
}

Parity parityFromIndex(int index)
{
    return static_cast<Parity>(qBound(0, index, 4));
}

int indexForParity(Parity parity)
{
    return qBound(0, static_cast<int>(parity), 4);
}

StopBits stopBitsFromIndex(int index)
{
    return static_cast<StopBits>(qBound(0, index, 2) + 1);
}

int indexForStopBits(StopBits bits)
{
    return qBound(0, static_cast<int>(bits) - 1, 2);
}

FlowControl flowControlFromIndex(int index)
{
    return static_cast<FlowControl>(qBound(0, index, 2));
}

int indexForFlowControl(FlowControl flowControl)
{
    return qBound(0, static_cast<int>(flowControl), 2);
}

int delimiterIndex(const QByteArray& delimiter)
{
    if (delimiter == QByteArrayLiteral("\r\n"))
        return 0;
    if (delimiter == QByteArrayLiteral("\n"))
        return 1;
    if (delimiter == QByteArrayLiteral("\r"))
        return 2;
    return 3;
}

QByteArray standardDelimiter(int index)
{
    switch (index) {
    case 0:
        return QByteArrayLiteral("\r\n");
    case 1:
        return QByteArrayLiteral("\n");
    case 2:
        return QByteArrayLiteral("\r");
    default:
        return {};
    }
}

} // namespace

SessionDialog::SessionDialog(QWidget* parent,
                             SerialPortProvider portProvider,
                             TransportKind preset,
                             const EndpointConfig* initial)
    : QDialog(parent)
    , m_portProvider(std::move(portProvider))
    , ui(new ::Ui::SessionDialog)
{
    ui->setupUi(this);

    if (QLineEdit* baudEdit = ui->baudCombo->lineEdit())
        baudEdit->setValidator(new QIntValidator(300, 4000000, this));

    connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->createButton, &QPushButton::clicked, this, &SessionDialog::accept);
    connect(ui->refreshButton, &QPushButton::clicked, this, [this]() {
        refreshPorts();
    });
    connect(ui->kindCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                ui->pages->setCurrentIndex(index);
                ui->errorLabel->setVisible(false);
            });
    connect(ui->frameModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { updateFramingVisibility(); });
    connect(ui->delimiterCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { updateFramingVisibility(); });

    refreshPorts();

    EndpointConfig config;
    if (initial) {
        config = *initial;
    } else {
        config.kind = preset;
        if (preset == TransportKind::TcpServer)
            config.localPort = 9000;
        if (preset == TransportKind::Udp) {
            config.localPort = 9000;
            config.host = QStringLiteral("127.0.0.1");
            config.remotePort = 9000;
        }
    }

    if (config.kind == TransportKind::Serial && config.portName.trimmed().isEmpty()
        && ui->portCombo->count() > 0) {
        config.portName = ui->portCombo->itemData(0).toString();
    }

    applyConfig(config);
}

SessionDialog::~SessionDialog()
{
    delete ui;
}

void SessionDialog::refreshPorts()
{
    const QString current = ui->portCombo->currentData().toString();
    const QVector<SerialPortInfo> ports = m_portProvider
        ? m_portProvider()
        : QVector<SerialPortInfo>();

    ui->portCombo->clear();
    for (const SerialPortInfo& port : ports)
        ui->portCombo->addItem(port.label(), port.name);

    if (ports.isEmpty())
        ui->portCombo->addItem(QStringLiteral("未检测到可用串口"), QString());

    if (!current.isEmpty() && ui->portCombo->findData(current) < 0)
        ui->portCombo->addItem(current, current);

    if (!current.isEmpty()) {
        const int index = ui->portCombo->findData(current);
        if (index >= 0)
            ui->portCombo->setCurrentIndex(index);
    }
}

void SessionDialog::applyConfig(const EndpointConfig& config)
{
    const int kindIndex = static_cast<int>(config.kind);
    ui->kindCombo->setCurrentIndex(kindIndex);
    ui->pages->setCurrentIndex(kindIndex);
    ui->nameEdit->setText(config.name);

    if (!config.portName.trimmed().isEmpty()) {
        int index = ui->portCombo->findData(config.portName);
        if (index < 0) {
            ui->portCombo->addItem(config.portName, config.portName);
            index = ui->portCombo->findData(config.portName);
        }
        ui->portCombo->setCurrentIndex(index);
    }

    ui->baudCombo->setCurrentText(QString::number(config.baudRate));
    ui->dataBitsCombo->setCurrentIndex(indexForDataBits(config.dataBits));
    ui->parityCombo->setCurrentIndex(indexForParity(config.parity));
    ui->stopBitsCombo->setCurrentIndex(indexForStopBits(config.stopBits));
    ui->flowControlCombo->setCurrentIndex(indexForFlowControl(config.flowControl));

    ui->tcpHostEdit->setText(config.host);
    ui->tcpPortSpin->setValue(config.remotePort > 0 ? config.remotePort : 8080);
    ui->connectTimeoutSpin->setValue(qBound(500, config.connectTimeoutMs, 60000));

    ui->listenAddressEdit->setText(config.bindAddress);
    ui->listenPortSpin->setValue(config.localPort > 0 ? config.localPort : 9000);

    ui->udpLocalPortSpin->setValue(config.localPort);
    ui->udpRemoteHostEdit->setText(config.host);
    ui->udpRemotePortSpin->setValue(config.remotePort);

    ui->frameModeCombo->setCurrentIndex(static_cast<int>(config.frameMode));
    if (config.frameMode == FrameMode::Line) {
        const int index = delimiterIndex(config.delimiter);
        ui->delimiterCombo->setCurrentIndex(index);
        if (index == 3)
            ui->customDelimiterEdit->setText(Bytes::toHex(config.delimiter));
    }
    ui->idleGapSpin->setValue(qBound(1, config.idleGapMs, 5000));

    updateFramingVisibility();
}

void SessionDialog::updateFramingVisibility()
{
    const int mode = ui->frameModeCombo->currentIndex();
    const bool line = mode == 1;
    const bool idle = mode == 2;

    ui->delimiterRow->setVisible(line);
    ui->customDelimiterRow->setVisible(line && ui->delimiterCombo->currentIndex() == 3);
    ui->idleRow->setVisible(idle);

    switch (mode) {
    case 0:
        ui->frameHint->setText(
            QStringLiteral("每次收到的数据块直接作为一条报文，适合二进制协议或自行切分。"));
        break;
    case 1:
        ui->frameHint->setText(
            QStringLiteral("按分隔符切分报文，分隔符会保留在报文内容中。"));
        break;
    default:
        ui->frameHint->setText(
            QStringLiteral("总线静默超过设定时间即认为一帧结束，适合 Modbus RTU 等无分隔协议。"));
        break;
    }
}

bool SessionDialog::validate(QString* error) const
{
    const int kind = ui->kindCombo->currentIndex();

    if (kind == static_cast<int>(TransportKind::Serial)) {
        const QString port = ui->portCombo->currentData().toString();
        if (port.trimmed().isEmpty()) {
            *error = QStringLiteral("请选择一个可用串口，然后点击“刷新”重新扫描。");
            return false;
        }
        bool ok = false;
        const int baud = ui->baudCombo->currentText().trimmed().toInt(&ok);
        if (!ok || baud < 300 || baud > 4000000) {
            *error = QStringLiteral("波特率无效，请输入 300 ~ 4000000 之间的整数。");
            return false;
        }
    } else if (kind == static_cast<int>(TransportKind::TcpClient)) {
        if (ui->tcpHostEdit->text().trimmed().isEmpty()) {
            *error = QStringLiteral("TCP 客户端需要填写目标地址。");
            return false;
        }
        if (ui->tcpPortSpin->value() <= 0) {
            *error = QStringLiteral("TCP 目标端口无效。");
            return false;
        }
    } else if (kind == static_cast<int>(TransportKind::TcpServer)) {
        const QString bind = ui->listenAddressEdit->text().trimmed();
        if (!bind.isEmpty()) {
            QHostAddress address;
            if (!address.setAddress(bind)) {
                *error = QStringLiteral("监听地址无效，请填写本机 IP（例如 0.0.0.0 或 192.168.1.10）。");
                return false;
            }
        }
        if (ui->listenPortSpin->value() <= 0) {
            *error = QStringLiteral("监听端口无效。");
            return false;
        }
    } else if (kind == static_cast<int>(TransportKind::Udp)) {
        const QString remoteHost = ui->udpRemoteHostEdit->text().trimmed();
        if (!remoteHost.isEmpty() && ui->udpRemotePortSpin->value() == 0) {
            *error = QStringLiteral("填写了 UDP 目标地址时，目标端口不能为空。");
            return false;
        }
    }

    if (ui->frameModeCombo->currentIndex() == static_cast<int>(FrameMode::Line)) {
        if (ui->delimiterCombo->currentIndex() == 3) {
            QByteArray delimiter;
            QString hexError;
            if (!Bytes::fromHex(ui->customDelimiterEdit->text(), &delimiter, &hexError)) {
                *error = QStringLiteral("自定义分隔符错误：%1").arg(hexError);
                return false;
            }
            if (delimiter.isEmpty()) {
                *error = QStringLiteral("自定义分隔符不能为空。");
                return false;
            }
        }
    }

    return true;
}

EndpointConfig SessionDialog::collectConfig() const
{
    EndpointConfig config;
    config.kind = static_cast<TransportKind>(ui->kindCombo->currentIndex());
    config.name = ui->nameEdit->text().trimmed();

    switch (config.kind) {
    case TransportKind::Serial:
        config.portName = ui->portCombo->currentData().toString();
        config.baudRate = ui->baudCombo->currentText().trimmed().toInt();
        config.dataBits = dataBitsFromIndex(ui->dataBitsCombo->currentIndex());
        config.parity = parityFromIndex(ui->parityCombo->currentIndex());
        config.stopBits = stopBitsFromIndex(ui->stopBitsCombo->currentIndex());
        config.flowControl = flowControlFromIndex(ui->flowControlCombo->currentIndex());
        break;
    case TransportKind::TcpClient:
        config.host = ui->tcpHostEdit->text().trimmed();
        config.remotePort = static_cast<quint16>(ui->tcpPortSpin->value());
        config.connectTimeoutMs = ui->connectTimeoutSpin->value();
        break;
    case TransportKind::TcpServer:
        config.bindAddress = ui->listenAddressEdit->text().trimmed();
        config.localPort = static_cast<quint16>(ui->listenPortSpin->value());
        break;
    case TransportKind::Udp:
        config.localPort = static_cast<quint16>(ui->udpLocalPortSpin->value());
        config.host = ui->udpRemoteHostEdit->text().trimmed();
        config.remotePort = config.host.isEmpty()
            ? 0
            : static_cast<quint16>(ui->udpRemotePortSpin->value());
        break;
    }

    config.frameMode = static_cast<FrameMode>(ui->frameModeCombo->currentIndex());
    if (config.frameMode == FrameMode::Line) {
        if (ui->delimiterCombo->currentIndex() == 3) {
            QByteArray delimiter;
            QString error;
            if (Bytes::fromHex(ui->customDelimiterEdit->text(), &delimiter, &error))
                config.delimiter = delimiter;
        } else {
            config.delimiter = standardDelimiter(ui->delimiterCombo->currentIndex());
        }
    }
    if (config.frameMode == FrameMode::IdleGap)
        config.idleGapMs = ui->idleGapSpin->value();

    return config;
}

void SessionDialog::accept()
{
    QString error;
    if (!validate(&error)) {
        ui->errorLabel->setText(error);
        ui->errorLabel->setVisible(true);
        return;
    }

    ui->errorLabel->setVisible(false);
    m_result = collectConfig();
    QDialog::accept();
}

} // namespace ui
} // namespace doulink
