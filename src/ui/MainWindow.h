#pragma once

#include "core/Types.h"
#include "ui/Theme.h"

#include <QHash>
#include <QMainWindow>
#include <QPoint>
#include <QVector>

class QTimer;
class QListWidgetItem;

namespace Ui {
class MainWindow;
}

namespace doulink {
class Session;
class SessionManager;

namespace ui {
class SessionListDelegate;
}
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(doulink::SessionManager* manager, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNewConnection();
    void onSessionAdded(doulink::Session* session);
    void onSessionRemoved(doulink::SessionId id);
    void onCurrentSessionChanged();
    void onSessionStateChanged(doulink::Session* session);
    void onSessionLogAppended(doulink::Session* session, const doulink::LogEntry& entry);
    void onSessionStatsChanged(doulink::Session* session);
    void onSessionNameChanged(doulink::Session* session);
    void onHistoryCleared(doulink::Session* session);
    void onTypeFilterChanged();
    void onSessionItemChanged(QListWidgetItem* item);
    void onThemeToggled();

    void onToggleSession();
    void onOpenAll();
    void onCloseAll();
    void onSendRequested(const QByteArray& payload, const QByteArray& suffix);

    void onClearLog();
    void onSaveLog();
    void onLogFilterChanged();
    void onHexToggled(bool checked);
    void onTimestampToggled(bool checked);

    void onListContextMenu(const QPoint& pos);
    void onRenameSession();
    void onDuplicateSession();
    void onRemoveSession();
    void onAbout();

private:
    void openDialog(doulink::TransportKind preset, const doulink::EndpointConfig* initial = nullptr);
    void addSessionToUi(doulink::Session* session);
    void connectSessionSignals(doulink::Session* session);
    void refreshSessionItem(doulink::Session* session);
    void applySessionFilter();
    void selectFirstVisibleSession();
    QVector<doulink::SessionId> checkedSessionIds() const;
    int checkedOnlineCount() const;

    doulink::Session* currentSession() const;
    void selectSession(doulink::SessionId id);
    void showEmptyState();
    void refreshHeader();
    void refreshSendAvailability();
    void refreshCounters();
    void syncCurrentSession();
    void showToast(const QString& message);
    QString suggestedLogFileName() const;
    void applyTheme(doulink::ui::Theme theme);
    void updateThemeButton();

    Ui::MainWindow* ui = nullptr;
    doulink::SessionManager* m_manager = nullptr;
    QTimer* m_toastTimer = nullptr;

    QHash<doulink::SessionId, QListWidgetItem*> m_items;
    doulink::SessionId m_currentId = doulink::kInvalidSessionId;
    doulink::ui::SessionListDelegate* m_sessionDelegate = nullptr;
    doulink::ui::Theme m_theme = doulink::ui::Theme::Dark;
};
