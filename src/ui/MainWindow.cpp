#include "ui/MainWindow.h"

#include "app/Session.h"
#include "app/SessionManager.h"
#include "core/ByteCodec.h"
#include "ui/LogView.h"
#include "ui/SendPanel.h"
#include "ui/SessionDialog.h"
#include "ui/SessionListDelegate.h"
#include "ui/Theme.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QChar>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QVariant>
#include <QWidget>

using doulink::ui::EndpointRole;
using doulink::ui::NameRole;
using doulink::ui::RxBytesRole;
using doulink::ui::KindRole;
using doulink::ui::SessionIdRole;
using doulink::ui::StateRole;
using doulink::ui::StateTextRole;
using doulink::ui::TxBytesRole;

MainWindow::MainWindow(doulink::SessionManager* manager, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_manager(manager)
{
    ui->setupUi(this);

    m_sessionDelegate = new doulink::ui::SessionListDelegate(this);
    ui->sessionList->setItemDelegate(m_sessionDelegate);
    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);
    ui->mainSplitter->setSizes({310, 1010});

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(3500);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() {
        ui->statusToast->clear();
    });

    connect(ui->newButton, &QPushButton::clicked, this, &MainWindow::onNewConnection);
    connect(ui->sideNewButton, &QPushButton::clicked, this, &MainWindow::onNewConnection);
    connect(ui->openAllButton, &QPushButton::clicked, this, &MainWindow::onOpenAll);
    connect(ui->closeAllButton, &QPushButton::clicked, this, &MainWindow::onCloseAll);
    connect(ui->aboutButton, &QPushButton::clicked, this, &MainWindow::onAbout);
    connect(ui->themeButton, &QPushButton::clicked, this, &MainWindow::onThemeToggled);
    connect(ui->toggleButton, &QPushButton::clicked, this, &MainWindow::onToggleSession);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(ui->saveButton, &QPushButton::clicked, this, &MainWindow::onSaveLog);
    connect(ui->hexToggle, &QCheckBox::toggled, this, &MainWindow::onHexToggled);
    connect(ui->timeToggle, &QCheckBox::toggled, this, &MainWindow::onTimestampToggled);
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &MainWindow::onLogFilterChanged);
    connect(ui->directionFilter,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { onLogFilterChanged(); });

    connect(ui->sessionList,
            &QListWidget::currentRowChanged,
            this,
            &MainWindow::onCurrentSessionChanged);
    connect(ui->sessionList,
            &QListWidget::customContextMenuRequested,
            this,
            &MainWindow::onListContextMenu);
    connect(ui->typeFilterCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { onTypeFilterChanged(); });
    connect(ui->sessionList,
            &QListWidget::itemChanged,
            this,
            &MainWindow::onSessionItemChanged);

    connect(ui->logView,
            &doulink::ui::LogView::countChanged,
            this,
            [this](int visible, int total) {
                ui->logMeta->setText(visible == total
                                         ? QStringLiteral("%1 条").arg(total)
                                         : QStringLiteral("显示 %1 / 共 %2 条").arg(visible).arg(total));
            });
    connect(ui->sendPanel,
            &doulink::ui::SendPanel::sendRequested,
            this,
            &MainWindow::onSendRequested);

    connect(ui->serialCard, &QPushButton::clicked, this, [this]() {
        openDialog(doulink::TransportKind::Serial);
    });
    connect(ui->tcpCard, &QPushButton::clicked, this, [this]() {
        openDialog(doulink::TransportKind::TcpClient);
    });
    connect(ui->udpCard, &QPushButton::clicked, this, [this]() {
        openDialog(doulink::TransportKind::Udp);
    });

    auto* newShortcut = new QShortcut(QKeySequence::New, this);
    newShortcut->setContext(Qt::WindowShortcut);
    connect(newShortcut, &QShortcut::activated, this, &MainWindow::onNewConnection);

    applyTheme(doulink::ui::savedTheme());

    if (m_manager) {
        connect(m_manager,
                &doulink::SessionManager::sessionAdded,
                this,
                &MainWindow::onSessionAdded);
        connect(m_manager,
                &doulink::SessionManager::sessionRemoved,
                this,
                &MainWindow::onSessionRemoved);

        const auto sessions = m_manager->sessions();
        for (doulink::Session* session : sessions)
            addSessionToUi(session);

        refreshCounters();

        refreshSendAvailability();

        if (!sessions.isEmpty()) {
            selectFirstVisibleSession();
            if (m_currentId == 0)
                showEmptyState();
        } else {
            showEmptyState();
        }
    } else {
        showEmptyState();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openDialog(doulink::TransportKind preset, const doulink::EndpointConfig* initial)
{
    if (!m_manager)
        return;

    ui->sendPanel->stopAutoSend();

    doulink::ui::SessionDialog dialog(
        this,
        [this]() { return m_manager->serialPorts(); },
        preset,
        initial);

    if (dialog.exec() != QDialog::Accepted)
        return;

    doulink::Session* session = m_manager->createSession(dialog.endpointConfig());
    if (!session) {
        QMessageBox::warning(this,
                             QStringLiteral("创建失败"),
                             QStringLiteral("无法创建传输层，请检查配置。"));
        return;
    }

    selectSession(session->id());
    session->open();
    showToast(QStringLiteral("已创建会话：%1").arg(session->name()));
}

void MainWindow::addSessionToUi(doulink::Session* session)
{
    if (!session || m_items.contains(session->id()))
        return;

    auto* item = new QListWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    item->setSizeHint(QSize(0, 78));
    item->setText(session->name());
    item->setData(SessionIdRole, QVariant::fromValue<qulonglong>(session->id()));

    m_items.insert(session->id(), item);
    ui->sessionList->addItem(item);
    connectSessionSignals(session);
    refreshSessionItem(session);
    applySessionFilter();
    refreshSendAvailability();

    if (m_currentId == 0)
        selectFirstVisibleSession();
}

void MainWindow::connectSessionSignals(doulink::Session* session)
{
    connect(session, &doulink::Session::stateChanged, this, &MainWindow::onSessionStateChanged);
    connect(session, &doulink::Session::logAppended, this, &MainWindow::onSessionLogAppended);
    connect(session, &doulink::Session::statsChanged, this, &MainWindow::onSessionStatsChanged);
    connect(session, &doulink::Session::nameChanged, this, &MainWindow::onSessionNameChanged);
    connect(session, &doulink::Session::historyCleared, this, &MainWindow::onHistoryCleared);
}

void MainWindow::refreshSessionItem(doulink::Session* session)
{
    if (!session)
        return;

    QListWidgetItem* item = m_items.value(session->id(), nullptr);
    if (!item)
        return;

    const doulink::SessionStats stats = session->stats();
    item->setText(session->name());
    item->setToolTip(QStringLiteral("%1\n%2").arg(session->name(), session->endpointText()));
    item->setData(NameRole, session->name());
    item->setData(EndpointRole, session->endpointText());
    item->setData(StateRole, static_cast<int>(session->state()));
    item->setData(StateTextRole, session->stateText());
    item->setData(RxBytesRole, QVariant::fromValue<qulonglong>(stats.rxBytes));
    item->setData(TxBytesRole, QVariant::fromValue<qulonglong>(stats.txBytes));
    item->setData(KindRole, static_cast<int>(session->config().kind));
    ui->sessionList->viewport()->update();
}

void MainWindow::applySessionFilter()
{
    const int filter = ui->typeFilterCombo->currentIndex();

    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        QListWidgetItem* item = it.value();
        const int kind = item->data(KindRole).toInt();
        bool visible = filter == 0;
        if (filter == 1) {
            visible = kind == static_cast<int>(doulink::TransportKind::Serial);
        } else if (filter == 2) {
            visible = kind == static_cast<int>(doulink::TransportKind::TcpClient)
                || kind == static_cast<int>(doulink::TransportKind::TcpServer);
        } else if (filter == 3) {
            visible = kind == static_cast<int>(doulink::TransportKind::Udp);
        }
        item->setHidden(!visible);
    }

    QListWidgetItem* current = ui->sessionList->currentItem();
    if (current && current->isHidden())
        selectFirstVisibleSession();
}

void MainWindow::selectFirstVisibleSession()
{
    for (int i = 0; i < ui->sessionList->count(); ++i) {
        QListWidgetItem* item = ui->sessionList->item(i);
        if (!item || item->isHidden())
            continue;
        const auto id = static_cast<doulink::SessionId>(item->data(SessionIdRole).toULongLong());
        selectSession(id);
        return;
    }
}

QVector<doulink::SessionId> MainWindow::checkedSessionIds() const
{
    QVector<doulink::SessionId> ids;
    for (int i = 0; i < ui->sessionList->count(); ++i) {
        QListWidgetItem* item = ui->sessionList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            ids.append(
                static_cast<doulink::SessionId>(item->data(SessionIdRole).toULongLong()));
        }
    }
    return ids;
}

int MainWindow::checkedOnlineCount() const
{
    if (!m_manager)
        return 0;

    int count = 0;
    for (doulink::SessionId id : checkedSessionIds()) {
        if (doulink::Session* session = m_manager->session(id)) {
            if (session->isOnline())
                ++count;
        }
    }
    return count;
}

doulink::Session* MainWindow::currentSession() const
{
    if (!m_manager || m_currentId == 0)
        return nullptr;
    return m_manager->session(m_currentId);
}

void MainWindow::selectSession(doulink::SessionId id)
{
    QListWidgetItem* item = m_items.value(id, nullptr);
    if (!item)
        return;
    ui->sessionList->setCurrentItem(item);
    ui->sessionList->scrollToItem(item);
}

void MainWindow::syncCurrentSession()
{
    QListWidgetItem* item = ui->sessionList->currentItem();
    doulink::Session* session = nullptr;
    if (item && m_manager) {
        const auto id = static_cast<doulink::SessionId>(item->data(SessionIdRole).toULongLong());
        session = m_manager->session(id);
        if (session)
            m_currentId = id;
    }

    if (!session) {
        showEmptyState();
        return;
    }

    ui->stack->setCurrentIndex(1);
    ui->logView->setEntries(session->history());

    refreshHeader();
    refreshSendAvailability();
}

void MainWindow::showEmptyState()
{
    m_currentId = 0;
    if (ui->stack)
        ui->stack->setCurrentIndex(0);
    if (ui->sendPanel)
        ui->sendPanel->setCanSend(0);
    setWindowTitle(QStringLiteral("DouLink · 串口 / 网络调试助手"));
}

void MainWindow::refreshHeader()
{
    doulink::Session* session = currentSession();
    if (!session)
        return;

    ui->nameLabel->setText(session->name());
    ui->endpointLabel->setText(session->endpointText());
    ui->endpointLabel->setToolTip(session->endpointText());
    ui->stateBadge->setText(session->stateText());

    QString stateKey;
    switch (session->state()) {
    case doulink::SessionState::Connected:
    case doulink::SessionState::Listening:
        stateKey = QStringLiteral("online");
        break;
    case doulink::SessionState::Connecting:
        stateKey = QStringLiteral("connecting");
        break;
    case doulink::SessionState::Error:
        stateKey = QStringLiteral("error");
        break;
    case doulink::SessionState::Disconnected:
        stateKey = QStringLiteral("offline");
        break;
    }
    if (ui->stateBadge->property("state").toString() != stateKey) {
        ui->stateBadge->setProperty("state", stateKey);
        ui->stateBadge->style()->unpolish(ui->stateBadge);
        ui->stateBadge->style()->polish(ui->stateBadge);
        ui->stateBadge->update();
    }

    const bool online = session->isOnline();
    const bool connecting = session->state() == doulink::SessionState::Connecting;

    QString buttonText;
    QString buttonRole;
    if (connecting) {
        buttonText = QStringLiteral("取消连接");
        buttonRole = QStringLiteral("ghost");
    } else if (online) {
        buttonText = QStringLiteral("断开连接");
        buttonRole = QStringLiteral("ghost");
    } else if (session->state() == doulink::SessionState::Error) {
        buttonText = QStringLiteral("重新连接");
        buttonRole = QStringLiteral("primary");
    } else {
        buttonText = QStringLiteral("连接");
        buttonRole = QStringLiteral("primary");
    }

    ui->toggleButton->setText(buttonText);
    if (ui->toggleButton->property("role").toString() != buttonRole) {
        ui->toggleButton->setProperty("role", buttonRole);
        ui->toggleButton->style()->unpolish(ui->toggleButton);
        ui->toggleButton->style()->polish(ui->toggleButton);
    }

    const doulink::SessionStats stats = session->stats();
    ui->rxStat->setText(QStringLiteral("RX  %1 · %2 帧")
                            .arg(doulink::Bytes::formatSize(stats.rxBytes))
                            .arg(stats.rxFrames));
    ui->txStat->setText(QStringLiteral("TX  %1 · %2 帧")
                            .arg(doulink::Bytes::formatSize(stats.txBytes))
                            .arg(stats.txFrames));

    const auto rateText = [](double value) {
        return doulink::Bytes::formatSize(static_cast<quint64>(value < 0.0 ? 0.0 : value + 0.5));
    };
    ui->rateStat->setText(QStringLiteral("速率  ↓ %1/s  ↑ %2/s")
                              .arg(rateText(stats.rxRate), rateText(stats.txRate)));
    ui->peerStat->setText(QStringLiteral("对端  %1").arg(stats.peers));

    const QString title = QStringLiteral("DouLink · %1").arg(session->name());
    if (windowTitle() != title)
        setWindowTitle(title);
}

void MainWindow::refreshSendAvailability()
{
    if (!ui->sendPanel)
        return;
    ui->sendPanel->setCanSend(checkedOnlineCount());
}

void MainWindow::refreshCounters()
{
    if (!m_manager)
        return;

    const int total = m_manager->sessions().size();
    const int online = m_manager->onlineCount();

    int visible = 0;
    for (int i = 0; i < ui->sessionList->count(); ++i) {
        QListWidgetItem* item = ui->sessionList->item(i);
        if (item && !item->isHidden())
            ++visible;
    }

    if (ui->typeFilterCombo->currentIndex() == 0) {
        ui->sessionCountBadge->setText(QString::number(total));
        ui->sessionCountBadge->setToolTip(QStringLiteral("会话总数：%1").arg(total));
    } else {
        ui->sessionCountBadge->setText(QStringLiteral("%1/%2").arg(visible).arg(total));
        ui->sessionCountBadge->setToolTip(
            QStringLiteral("当前过滤显示 %1 个，共 %2 个").arg(visible).arg(total));
    }

    ui->statusInfo->setText(QStringLiteral("%1 个会话 · %2 个在线").arg(total).arg(online));
}

void MainWindow::onThemeToggled()
{
    applyTheme(m_theme == doulink::ui::Theme::Light
                   ? doulink::ui::Theme::Dark
                   : doulink::ui::Theme::Light);
}

void MainWindow::applyTheme(doulink::ui::Theme theme)
{
    m_theme = theme;
    doulink::ui::applyTheme(*qApp, theme);

    if (m_sessionDelegate)
        m_sessionDelegate->setTheme(theme);
    if (ui->sessionList)
        ui->sessionList->viewport()->update();
    if (ui->logView)
        ui->logView->setTheme(theme);
    if (ui->sendPanel)
        ui->sendPanel->refreshTheme();

    updateThemeButton();
    doulink::ui::saveTheme(theme);
}

void MainWindow::updateThemeButton()
{
    if (!ui->themeButton)
        return;

    const bool light = m_theme == doulink::ui::Theme::Light;
    ui->themeButton->setText(light ? QStringLiteral("暗色") : QStringLiteral("亮色"));
    ui->themeButton->setToolTip(light ? QStringLiteral("切换到暗色主题")
                                      : QStringLiteral("切换到亮色主题"));
}

void MainWindow::onNewConnection()
{
    openDialog(doulink::TransportKind::Serial);
}

void MainWindow::onSessionAdded(doulink::Session* session)
{
    addSessionToUi(session);
    refreshCounters();
    refreshSendAvailability();
}

void MainWindow::onSessionRemoved(doulink::SessionId id)
{
    if (QListWidgetItem* item = m_items.take(id)) {
        const int row = ui->sessionList->row(item);
        if (row >= 0)
            delete ui->sessionList->takeItem(row);
    }

    if (m_currentId == id)
        m_currentId = 0;

    applySessionFilter();

    if (m_currentId == 0)
        selectFirstVisibleSession();
    if (m_currentId == 0)
        showEmptyState();

    refreshCounters();
    refreshSendAvailability();
}

void MainWindow::onCurrentSessionChanged()
{
    syncCurrentSession();
}

void MainWindow::onTypeFilterChanged()
{
    applySessionFilter();
    if (!ui->sessionList->currentItem())
        selectFirstVisibleSession();
    refreshCounters();
}

void MainWindow::onSessionItemChanged(QListWidgetItem* item)
{
    Q_UNUSED(item);
    refreshSendAvailability();
}

void MainWindow::onSessionStateChanged(doulink::Session* session)
{
    if (!session)
        return;
    refreshSessionItem(session);
    refreshCounters();
    if (session->id() == m_currentId)
        refreshHeader();
    refreshSendAvailability();
}

void MainWindow::onSessionLogAppended(doulink::Session* session, const doulink::LogEntry& entry)
{
    if (!session || session->id() != m_currentId)
        return;
    if (ui->stack->currentIndex() != 1)
        return;
    ui->logView->appendEntry(entry);
}

void MainWindow::onSessionStatsChanged(doulink::Session* session)
{
    if (!session)
        return;
    refreshSessionItem(session);
    if (session->id() == m_currentId)
        refreshHeader();
}

void MainWindow::onSessionNameChanged(doulink::Session* session)
{
    if (!session)
        return;
    refreshSessionItem(session);
    if (session->id() == m_currentId)
        refreshHeader();
}

void MainWindow::onHistoryCleared(doulink::Session* session)
{
    if (session && session->id() == m_currentId)
        ui->logView->clearEntries();
}

void MainWindow::onToggleSession()
{
    doulink::Session* session = currentSession();
    if (!session)
        return;

    if (session->isOnline() || session->state() == doulink::SessionState::Connecting)
        session->close();
    else
        session->open();
}

void MainWindow::onOpenAll()
{
    if (!m_manager)
        return;
    m_manager->openAll();
    refreshCounters();
    showToast(QStringLiteral("已请求连接所有会话"));
}

void MainWindow::onCloseAll()
{
    if (!m_manager)
        return;
    m_manager->closeAll();
    refreshCounters();
    showToast(QStringLiteral("已断开所有会话"));
}

void MainWindow::onSendRequested(const QByteArray& payload, const QByteArray& suffix)
{
    if (!m_manager)
        return;

    const QVector<doulink::SessionId> ids = checkedSessionIds();
    if (ids.isEmpty()) {
        showToast(QStringLiteral("请先勾选要发送的设备"));
        return;
    }

    const int delivered = m_manager->sendTo(ids, payload, suffix);
    if (delivered <= 0) {
        ui->sendPanel->stopAutoSend();
        showToast(QStringLiteral("发送失败：勾选的设备均不在线或不可写"));
        return;
    }

    if (!ui->sendPanel->isAutoSendActive())
        showToast(QStringLiteral("已发送到 %1 个设备").arg(delivered));
}

void MainWindow::onClearLog()
{
    doulink::Session* session = currentSession();
    if (!session)
        return;
    session->clearHistory();
    showToast(QStringLiteral("报文日志已清空"));
}

void MainWindow::onSaveLog()
{
    doulink::Session* session = currentSession();
    if (!session)
        return;

    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存报文日志"),
        suggestedLogFileName(),
        QStringLiteral("日志文件 (*.log *.txt);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this,
                             QStringLiteral("保存失败"),
                             QStringLiteral("无法写入文件：%1").arg(file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << QStringLiteral("# DouLink 报文日志\n");
    stream << QStringLiteral("# 会话: ") << session->name() << QLatin1Char('\n');
    stream << QStringLiteral("# 端点: ") << session->endpointText() << QLatin1Char('\n');
    const doulink::SessionStats stats = session->stats();
    stream << QStringLiteral("# 统计: RX ")
           << doulink::Bytes::formatSize(stats.rxBytes)
           << QStringLiteral(" / ") << stats.rxFrames << QStringLiteral(" 帧, TX ")
           << doulink::Bytes::formatSize(stats.txBytes)
           << QStringLiteral(" / ") << stats.txFrames << QStringLiteral(" 帧\n");
    stream << QStringLiteral("# 导出时间: ")
           << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
           << QLatin1Char('\n');
    stream << QStringLiteral("# 说明: [时间] 方向 [对端] 内容\n\n");
    stream << ui->logView->exportText();
    file.close();

    showToast(QStringLiteral("日志已保存：%1").arg(path));
}

void MainWindow::onLogFilterChanged()
{
    ui->logView->setFilter(ui->searchEdit->text(), ui->directionFilter->currentIndex());
}

void MainWindow::onHexToggled(bool checked)
{
    ui->logView->setDataFormat(checked ? doulink::DataFormat::Hex : doulink::DataFormat::Text);
}

void MainWindow::onTimestampToggled(bool checked)
{
    ui->logView->setShowTimestamp(checked);
}

void MainWindow::onListContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = ui->sessionList->itemAt(pos);
    if (!item)
        return;

    ui->sessionList->setCurrentItem(item);
    doulink::Session* session = currentSession();
    if (!session)
        return;

    QMenu menu(this);
    const bool active = session->isOnline()
        || session->state() == doulink::SessionState::Connecting;
    QAction* toggleAction = menu.addAction(active ? QStringLiteral("断开连接")
                                                  : QStringLiteral("连接"));
    QAction* renameAction = menu.addAction(QStringLiteral("重命名"));
    QAction* duplicateAction = menu.addAction(QStringLiteral("复制为新连接"));
    QAction* clearAction = menu.addAction(QStringLiteral("清空日志"));
    menu.addSeparator();
    QAction* removeAction = menu.addAction(QStringLiteral("删除会话"));

    QAction* chosen = menu.exec(ui->sessionList->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == toggleAction)
        onToggleSession();
    else if (chosen == renameAction)
        onRenameSession();
    else if (chosen == duplicateAction)
        onDuplicateSession();
    else if (chosen == clearAction)
        onClearLog();
    else if (chosen == removeAction)
        onRemoveSession();
}

void MainWindow::onRenameSession()
{
    doulink::Session* session = currentSession();
    if (!session)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("重命名会话"),
                                               QStringLiteral("会话名称"),
                                               QLineEdit::Normal,
                                               session->name(),
                                               &ok);
    if (!ok)
        return;
    if (name.trimmed().isEmpty()) {
        showToast(QStringLiteral("会话名称不能为空"));
        return;
    }
    session->setName(name);
}

void MainWindow::onDuplicateSession()
{
    doulink::Session* session = currentSession();
    if (!session)
        return;
    openDialog(session->config().kind, &session->config());
}

void MainWindow::onRemoveSession()
{
    doulink::Session* session = currentSession();
    if (!session)
        return;

    const QString question = session->isOnline()
        ? QStringLiteral("会话“%1”正在运行，删除将立即断开连接。确定删除？").arg(session->name())
        : QStringLiteral("确定删除会话“%1”？").arg(session->name());

    if (QMessageBox::question(this,
                              QStringLiteral("删除会话"),
                              question,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }

    const doulink::SessionId id = session->id();
    m_manager->removeSession(id);
    showToast(QStringLiteral("会话已删除"));
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this,
        QStringLiteral("关于 DouLink"),
        QString("<b>DouLink %1</b><br><br>"
                       "串口 / TCP / UDP 多设备调试助手<br>"
                       "分层架构 · 异步 I/O · 多会话并发<br>"
                       "时间戳与方向彩色报文日志<br><br>").arg(qApp->applicationVersion()));
}

void MainWindow::showToast(const QString& message)
{
    ui->statusToast->setText(message);
    m_toastTimer->start();
}

QString MainWindow::suggestedLogFileName() const
{
    doulink::Session* session = currentSession();
    QString name = session ? session->name() : QStringLiteral("DouLink");
    name = name.left(40);

    const QString invalid = QStringLiteral("\\/:*?\"<>|");
    for (int i = 0; i < name.size(); ++i) {
        if (invalid.contains(name.at(i)))
            name[i] = QChar(QLatin1Char('_'));
    }
    if (name.trimmed().isEmpty())
        name = QStringLiteral("session");

    return QStringLiteral("DouLink_%1_%2.log")
        .arg(name, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
}
