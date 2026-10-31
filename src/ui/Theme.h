#pragma once

#include <QColor>

class QApplication;

namespace doulink {
namespace ui {

enum class Theme {
    Dark,
    Light
};

// Colours used by custom-painted controls (session cards, log rows). QSS
// handles the rest of the widget tree.
struct ThemePalette {
    QColor logTimestamp;
    QColor logRxTag;
    QColor logRxTagBg;
    QColor logTxTag;
    QColor logTxTagBg;
    QColor logSysTag;
    QColor logSysTagBg;
    QColor logErrTag;
    QColor logErrTagBg;
    QColor logPeer;
    QColor logSysText;
    QColor logErrText;
    QColor logRxText;
    QColor logTxText;

    QColor cardBg;
    QColor cardHoverBg;
    QColor cardSelectedBg;
    QColor cardBorder;
    QColor cardSelectedBorder;
    QColor accent;
    QColor checkboxBorder;
    QColor checkboxBg;
    QColor checkboxCheckedBorder;
    QColor checkboxCheckedBg;
    QColor checkMark;
    QColor sessionName;
    QColor sessionEndpoint;
    QColor metricRx;
    QColor metricTx;
    QColor stateOnline;
    QColor stateConnecting;
    QColor stateError;
    QColor stateOffline;
};

const ThemePalette& themePalette(Theme theme);

// Applies the QSS resource and Fusion palette for the selected theme.
void applyTheme(QApplication& app, Theme theme);

Theme savedTheme();
void saveTheme(Theme theme);

} // namespace ui
} // namespace doulink
