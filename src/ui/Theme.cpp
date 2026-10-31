#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QString>
#include <QtGlobal>

namespace doulink {
namespace ui {
namespace {

ThemePalette darkPalette()
{
    ThemePalette p;
    p.logTimestamp = QColor(QStringLiteral("#5a7186"));
    p.logRxTag = QColor(QStringLiteral("#4ade80"));
    p.logRxTagBg = QColor(QStringLiteral("#10352a"));
    p.logTxTag = QColor(QStringLiteral("#38bdf8"));
    p.logTxTagBg = QColor(QStringLiteral("#0c2d40"));
    p.logSysTag = QColor(QStringLiteral("#94a3b8"));
    p.logSysTagBg = QColor(QStringLiteral("#1a2632"));
    p.logErrTag = QColor(QStringLiteral("#f87171"));
    p.logErrTagBg = QColor(QStringLiteral("#3a171b"));
    p.logPeer = QColor(QStringLiteral("#c4b5fd"));
    p.logSysText = QColor(QStringLiteral("#8fa2b3"));
    p.logErrText = QColor(QStringLiteral("#fca5a5"));
    p.logRxText = QColor(QStringLiteral("#c3f7d5"));
    p.logTxText = QColor(QStringLiteral("#c7e9ff"));

    p.cardBg = QColor(QStringLiteral("#0e1720"));
    p.cardHoverBg = QColor(QStringLiteral("#111c26"));
    p.cardSelectedBg = QColor(QStringLiteral("#162431"));
    p.cardBorder = QColor(QStringLiteral("#172531"));
    p.cardSelectedBorder = QColor(QStringLiteral("#2b5a70"));
    p.accent = QColor(QStringLiteral("#22d3ee"));
    p.checkboxBorder = QColor(QStringLiteral("#3b5570"));
    p.checkboxBg = QColor(QStringLiteral("#0b131c"));
    p.checkboxCheckedBorder = QColor(QStringLiteral("#22d3ee"));
    p.checkboxCheckedBg = QColor(QStringLiteral("#123243"));
    p.checkMark = QColor(QStringLiteral("#67e8f9"));
    p.sessionName = QColor(QStringLiteral("#e8f1f8"));
    p.sessionEndpoint = QColor(QStringLiteral("#7c93a7"));
    p.metricRx = QColor(QStringLiteral("#4ade80"));
    p.metricTx = QColor(QStringLiteral("#38bdf8"));
    p.stateOnline = QColor(QStringLiteral("#4ade80"));
    p.stateConnecting = QColor(QStringLiteral("#fbbf24"));
    p.stateError = QColor(QStringLiteral("#f87171"));
    p.stateOffline = QColor(QStringLiteral("#68798a"));
    return p;
}

ThemePalette lightPalette()
{
    ThemePalette p;
    p.logTimestamp = QColor(QStringLiteral("#5c6f82"));
    p.logRxTag = QColor(QStringLiteral("#166534"));
    p.logRxTagBg = QColor(QStringLiteral("#e8f6ec"));
    p.logTxTag = QColor(QStringLiteral("#0369a1"));
    p.logTxTagBg = QColor(QStringLiteral("#e0f2fe"));
    p.logSysTag = QColor(QStringLiteral("#475569"));
    p.logSysTagBg = QColor(QStringLiteral("#eef2f6"));
    p.logErrTag = QColor(QStringLiteral("#b91c1c"));
    p.logErrTagBg = QColor(QStringLiteral("#fdecec"));
    p.logPeer = QColor(QStringLiteral("#6d28d9"));
    p.logSysText = QColor(QStringLiteral("#475569"));
    p.logErrText = QColor(QStringLiteral("#b91c1c"));
    p.logRxText = QColor(QStringLiteral("#14532d"));
    p.logTxText = QColor(QStringLiteral("#0c4a6e"));

    p.cardBg = QColor(QStringLiteral("#ffffff"));
    p.cardHoverBg = QColor(QStringLiteral("#f1f7fa"));
    p.cardSelectedBg = QColor(QStringLiteral("#d9eef4"));
    p.cardBorder = QColor(QStringLiteral("#a2b6c8"));
    p.cardSelectedBorder = QColor(QStringLiteral("#4f9db5"));
    p.accent = QColor(QStringLiteral("#0e7490"));
    p.checkboxBorder = QColor(QStringLiteral("#829bb0"));
    p.checkboxBg = QColor(QStringLiteral("#ffffff"));
    p.checkboxCheckedBorder = QColor(QStringLiteral("#0e7490"));
    p.checkboxCheckedBg = QColor(QStringLiteral("#d9eef4"));
    p.checkMark = QColor(QStringLiteral("#0e7490"));
    p.sessionName = QColor(QStringLiteral("#1c3043"));
    p.sessionEndpoint = QColor(QStringLiteral("#5c6f82"));
    p.metricRx = QColor(QStringLiteral("#166534"));
    p.metricTx = QColor(QStringLiteral("#0369a1"));
    p.stateOnline = QColor(QStringLiteral("#166534"));
    p.stateConnecting = QColor(QStringLiteral("#92400e"));
    p.stateError = QColor(QStringLiteral("#b91c1c"));
    p.stateOffline = QColor(QStringLiteral("#5c6f82"));
    return p;
}

} // namespace

const ThemePalette& themePalette(Theme theme)
{
    static const ThemePalette dark = darkPalette();
    static const ThemePalette light = lightPalette();
    return theme == Theme::Light ? light : dark;
}

void applyTheme(QApplication& app, Theme theme)
{
    const bool light = theme == Theme::Light;

    QPalette palette = app.palette();
    palette.setColor(QPalette::Window,
                     QColor(light ? QStringLiteral("#eef2f7") : QStringLiteral("#0a1017")));
    palette.setColor(QPalette::WindowText,
                     QColor(light ? QStringLiteral("#243447") : QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::Base,
                     QColor(light ? QStringLiteral("#ffffff") : QStringLiteral("#0b131c")));
    palette.setColor(QPalette::AlternateBase,
                     QColor(light ? QStringLiteral("#f5f8fb") : QStringLiteral("#101a24")));
    palette.setColor(QPalette::ToolTipBase,
                     QColor(light ? QStringLiteral("#ffffff") : QStringLiteral("#0f1a24")));
    palette.setColor(QPalette::ToolTipText,
                     QColor(light ? QStringLiteral("#243447") : QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::Text,
                     QColor(light ? QStringLiteral("#34495e") : QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::Button,
                     QColor(light ? QStringLiteral("#ffffff") : QStringLiteral("#14202c")));
    palette.setColor(QPalette::ButtonText,
                     QColor(light ? QStringLiteral("#34495e") : QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::BrightText,
                     QColor(light ? QStringLiteral("#b91c1c") : QStringLiteral("#fda4af")));
    palette.setColor(QPalette::Highlight,
                     QColor(light ? QStringLiteral("#0e7490") : QStringLiteral("#0e7490")));
    palette.setColor(QPalette::HighlightedText,
                     QColor(light ? QStringLiteral("#ffffff") : QStringLiteral("#f0fdff")));
    palette.setColor(QPalette::Link,
                     QColor(light ? QStringLiteral("#0369a1") : QStringLiteral("#22d3ee")));
    palette.setColor(QPalette::LinkVisited,
                     QColor(light ? QStringLiteral("#6d28d9") : QStringLiteral("#a78bfa")));
    palette.setColor(QPalette::Light,
                     QColor(light ? QStringLiteral("#ffffff") : QStringLiteral("#2a3d50")));
    palette.setColor(QPalette::Midlight,
                     QColor(light ? QStringLiteral("#f5f8fb") : QStringLiteral("#172531")));
    palette.setColor(QPalette::Dark,
                     QColor(light ? QStringLiteral("#b3c5d4") : QStringLiteral("#05090d")));
    palette.setColor(QPalette::Mid,
                     QColor(light ? QStringLiteral("#6f8396") : QStringLiteral("#52677a")));
    palette.setColor(QPalette::Shadow,
                     QColor(light ? QStringLiteral("#5c6f82") : QStringLiteral("#000000")));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(light ? QStringLiteral("#6f8396") : QStringLiteral("#52677a")));
    palette.setColor(QPalette::Disabled, QPalette::Text,
                     QColor(light ? QStringLiteral("#6f8396") : QStringLiteral("#52677a")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(light ? QStringLiteral("#6f8396") : QStringLiteral("#52677a")));
    palette.setColor(QPalette::Disabled, QPalette::Base,
                     QColor(light ? QStringLiteral("#f0f3f6") : QStringLiteral("#0e151d")));
    palette.setColor(QPalette::Disabled, QPalette::Button,
                     QColor(light ? QStringLiteral("#e9edf1") : QStringLiteral("#101821")));
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    palette.setColor(QPalette::PlaceholderText,
                     QColor(light ? QStringLiteral("#6f8396") : QStringLiteral("#52677a")));
#endif
    app.setPalette(palette);

    QFile styleFile(light ? QStringLiteral(":/theme_light.qss")
                          : QStringLiteral(":/theme_dark.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    } else {
        app.setStyleSheet(QString());
    }

    app.setProperty("doulinkTheme", light ? QStringLiteral("light") : QStringLiteral("dark"));
}

Theme savedTheme()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    QString configPath = configDir + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    const QString value = settings.value(QStringLiteral("ui/theme"), QStringLiteral("dark")).toString();
    return value.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0
        ? Theme::Light
        : Theme::Dark;
}

void saveTheme(Theme theme)
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    QString configPath = configDir + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    
    settings.setValue(QStringLiteral("ui/theme"),
                      theme == Theme::Light ? QStringLiteral("light") : QStringLiteral("dark"));
}

} // namespace ui
} // namespace doulink
