// destruct-gui — a keyboard-first window over the DeStruct decompiler.

#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "formats.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("destruct"));
    app.setApplicationName(QStringLiteral("destruct-gui"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    // Matches the window to destruct-gui.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName(QStringLiteral("destruct-gui"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("destruct-gui")));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    // The desktop entry passes a file with %f, and two files on the command
    // line are the natural way to open an IL2CPP library and its metadata.
    for (const QString &argument : app.arguments().mid(1)) {
        if (argument.startsWith(QLatin1Char('-')))
            continue;
        const QFileInfo info(argument);
        if (!info.isFile())
            continue;
        const QString path = info.absoluteFilePath();
        if (formats::detect(path).kind == formats::Kind::Metadata)
            backend.setMetadataPath(path);
        else
            backend.setInputPath(path);
    }

    return app.exec();
}
