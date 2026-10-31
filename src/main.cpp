#include "app/SessionManager.h"
#include "infra/TransportFactory.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DouLink"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("DouLink"));

    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        QApplication::setStyle(fusion);

    // A dark Fusion palette keeps native sub-controls (check marks, spin arrows,
    // scrollbars) legible where the stylesheet does not paint them explicitly.
    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#0a1017")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#0b131c")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#101a24")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#0f1a24")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#14202c")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#dbe7f2")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#fda4af")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#0e7490")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#f0fdff")));
    palette.setColor(QPalette::Link, QColor(QStringLiteral("#22d3ee")));
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#52677a")));
#endif
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#52677a")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#52677a")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#52677a")));
    QApplication::setPalette(palette);

    QFont appFont(QStringLiteral("Microsoft YaHei UI"));
    appFont.setPointSize(9);
    QApplication::setFont(appFont);

    QFile styleFile(QStringLiteral(":/theme.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.png")));

    doulink::SessionManager manager(
        [](doulink::TransportKind kind) { return doulink::infra::createTransport(kind); },
        []() { return doulink::infra::availableSerialPorts(); });

    MainWindow window(&manager);
    window.show();

    return app.exec();
}
