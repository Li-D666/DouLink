#include "ui/SendPanel.h"

#include "core/ByteCodec.h"
#include "ui_sendpanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QKeySequence>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>

namespace doulink {
namespace ui {

SendPanel::SendPanel(QWidget* parent)
    : QFrame(parent)
    , ui(new ::Ui::SendPanel)
{
    ui->setupUi(this);

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::CoarseTimer);

    connect(ui->textButton, &QToolButton::clicked, this, [this]() {
        onFormatChanged();
    });
    connect(ui->hexButton, &QToolButton::clicked, this, [this]() {
        onFormatChanged();
    });
    connect(ui->input, &QPlainTextEdit::textChanged, this, &SendPanel::updateMeta);
    connect(ui->suffixCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &SendPanel::onSuffixChanged);
    connect(ui->customSuffix, &QLineEdit::textChanged, this, &SendPanel::updateMeta);
    connect(ui->autoCheck, &QCheckBox::toggled, this, &SendPanel::onAutoSendToggled);
    connect(ui->intervalSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int) {
                if (m_timer->isActive())
                    m_timer->start(ui->intervalSpin->value());
                updateMeta();
            });
    connect(m_timer, &QTimer::timeout, this, &SendPanel::fireTimer);
    connect(ui->sendButton, &QPushButton::clicked, this, [this]() {
        doSend();
    });

    auto* sendShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    sendShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(sendShortcut, &QShortcut::activated, this, &SendPanel::doSend);

    auto* sendShortcutEnter = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Enter")), this);
    sendShortcutEnter->setContext(Qt::WidgetWithChildrenShortcut);
    connect(sendShortcutEnter, &QShortcut::activated, this, &SendPanel::doSend);

    updateMeta();
}

SendPanel::~SendPanel()
{
    delete ui;
}

void SendPanel::setCanSend(int onlineTargetCount)
{
    m_targetCount = qMax(0, onlineTargetCount);

    if (m_targetCount == 0 && ui->autoCheck->isChecked())
        ui->autoCheck->setChecked(false);

    updateMeta();
}

void SendPanel::refreshTheme()
{
    ui->meta->setProperty("state",
                          m_metaError ? QVariant(QStringLiteral("error")) : QVariant());
    ui->meta->style()->unpolish(ui->meta);
    ui->meta->style()->polish(ui->meta);
    updateMeta();
}

void SendPanel::setMetaError(bool error)
{
    if (m_metaError == error)
        return;

    m_metaError = error;
    ui->meta->setProperty("state",
                          error ? QVariant(QStringLiteral("error")) : QVariant());
    ui->meta->style()->unpolish(ui->meta);
    ui->meta->style()->polish(ui->meta);
    ui->meta->update();
}

bool SendPanel::isAutoSendActive() const
{
    return m_timer->isActive();
}

void SendPanel::stopAutoSend()
{
    m_timer->stop();
    if (ui->autoCheck->isChecked())
        ui->autoCheck->setChecked(false);
    updateMeta();
}

void SendPanel::onFormatChanged()
{
    if (!ui->textButton->isChecked() && !ui->hexButton->isChecked())
        ui->textButton->setChecked(true);

    const bool hex = ui->hexButton->isChecked();
    ui->input->setPlaceholderText(
        hex ? QStringLiteral("输入十六进制，例如 01 03 00 00 00 0A\nCtrl+Enter 快速发送")
            : QStringLiteral("输入要发送的内容…\nCtrl+Enter 快速发送"));
    updateMeta();
}

void SendPanel::onSuffixChanged(int index)
{
    const bool custom = index == 4;
    ui->customSuffixLabel->setVisible(custom);
    ui->customSuffix->setVisible(custom);
    updateMeta();
}

void SendPanel::onAutoSendToggled(bool enabled)
{
    if (!enabled) {
        m_timer->stop();
        updateMeta();
        return;
    }

    QByteArray payload;
    QByteArray suffix;
    QString error;
    bool empty = false;
    if (m_targetCount == 0 || !buildPayload(&payload, &suffix, &error, &empty) || empty) {
        ui->autoCheck->setChecked(false);
        return;
    }

    m_timer->start(ui->intervalSpin->value());
    updateMeta();
}

void SendPanel::doSend()
{
    if (m_targetCount == 0) {
        updateMeta();
        return;
    }

    QByteArray payload;
    QByteArray suffix;
    QString error;
    bool empty = false;
    if (!buildPayload(&payload, &suffix, &error, &empty)) {
        updateMeta();
        return;
    }
    if (empty) {
        ui->input->setFocus();
        return;
    }

    emit sendRequested(payload, suffix);
}

void SendPanel::fireTimer()
{
    if (m_targetCount == 0) {
        ui->autoCheck->setChecked(false);
        return;
    }

    QByteArray payload;
    QByteArray suffix;
    QString error;
    bool empty = false;
    if (!buildPayload(&payload, &suffix, &error, &empty) || empty) {
        ui->autoCheck->setChecked(false);
        updateMeta();
        return;
    }

    emit sendRequested(payload, suffix);
}

void SendPanel::updateMeta()
{
    QByteArray payload;
    QByteArray suffix;
    QString error;
    bool empty = false;
    const bool parsed = buildPayload(&payload, &suffix, &error, &empty);

    if (!parsed) {
        setMetaError(true);
        ui->meta->setText(error);
        ui->sendButton->setEnabled(false);
        if (ui->autoCheck->isChecked())
            ui->autoCheck->setChecked(false);
        return;
    }

    if (empty) {
        QString text = QStringLiteral("等待输入…");
        if (m_targetCount > 0)
            text += QStringLiteral(" · 目标 %1 个在线设备").arg(m_targetCount);
        else
            text += QStringLiteral(" · 没有可发送的在线设备");

        setMetaError(false);
        ui->meta->setText(text);
        ui->sendButton->setEnabled(false);
        if (ui->autoCheck->isChecked())
            ui->autoCheck->setChecked(false);
        return;
    }

    const int bytes = payload.size() + suffix.size();
    QString text = QStringLiteral("%1 字节 · %2")
                       .arg(bytes)
                       .arg(ui->hexButton->isChecked() ? QStringLiteral("HEX")
                                                       : QStringLiteral("文本"));
    if (m_timer->isActive())
        text += QStringLiteral(" · 每 %1 ms 自动发送").arg(ui->intervalSpin->value());
    if (m_targetCount <= 0)
        text += QStringLiteral(" · 没有可发送的在线设备");
    else
        text += QStringLiteral(" · 目标 %1 个在线设备").arg(m_targetCount);

    setMetaError(false);
    ui->meta->setText(text);

    ui->sendButton->setEnabled(m_targetCount > 0);
}

bool SendPanel::buildPayload(QByteArray* payload,
                             QByteArray* suffix,
                             QString* error,
                             bool* empty) const
{
    payload->clear();
    suffix->clear();
    error->clear();
    *empty = false;

    switch (ui->suffixCombo->currentIndex()) {
    case 0:
        break;
    case 1:
        suffix->append("\r\n");
        break;
    case 2:
        suffix->append('\n');
        break;
    case 3:
        suffix->append('\r');
        break;
    case 4: {
        QString suffixError;
        if (!Bytes::fromHex(ui->customSuffix->text(), suffix, &suffixError)) {
            *error = QStringLiteral("结尾 HEX 错误：%1").arg(suffixError);
            return false;
        }
        break;
    }
    default:
        break;
    }

    if (ui->hexButton->isChecked()) {
        QString hexError;
        if (!Bytes::fromHex(ui->input->toPlainText(), payload, &hexError)) {
            *error = QStringLiteral("HEX 格式错误：%1").arg(hexError);
            return false;
        }
    } else {
        *payload = ui->input->toPlainText().toUtf8();
    }

    *empty = payload->isEmpty() && suffix->isEmpty();
    return true;
}

} // namespace ui
} // namespace doulink
