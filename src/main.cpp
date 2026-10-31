#include "app/SessionManager.h"
#include "infra/TransportFactory.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
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
    QApplication::setApplicationVersion(QStringLiteral("1.0.1"));
    QApplication::setOrganizationName(QStringLiteral("DouLink"));

    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        QApplication::setStyle(fusion);

    QFont appFont(QStringLiteral("Microsoft YaHei UI"));
    appFont.setPointSize(9);
    QApplication::setFont(appFont);

    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.png")));

    doulink::SessionManager manager(
        [](doulink::TransportKind kind) { return doulink::infra::createTransport(kind); },
        []() { return doulink::infra::availableSerialPorts(); });

    MainWindow window(&manager);
    window.show();

    return app.exec();
}
