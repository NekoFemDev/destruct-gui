// Portal file dialogs, adapted from Omacut (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace {
struct PortalFilterRule {
    uint type;
    QString pattern;
};

using PortalFilterRules = QList<PortalFilterRule>;

struct PortalFileFilter {
    QString name;
    PortalFilterRules rules;
};

using PortalFileFilters = QList<PortalFileFilter>;

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilterRule &rule)
{
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilterRule &rule)
{
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFileFilter &filter)
{
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFileFilter &filter)
{
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

void registerPortalFilterTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<PortalFilterRule>();
        qDBusRegisterMetaType<PortalFilterRules>();
        qDBusRegisterMetaType<PortalFileFilter>();
        qDBusRegisterMetaType<PortalFileFilters>();
        return true;
    }();
    Q_UNUSED(registered);
}

PortalFileFilters inputFilters()
{
    const QStringList patterns{
        QStringLiteral("*.jar"), QStringLiteral("*.class"),
        QStringLiteral("*.dex"), QStringLiteral("*.apk"),
        QStringLiteral("*.hbc"), QStringLiteral("*.bundle"),
        QStringLiteral("*.so"), QStringLiteral("*.elf"),
        QStringLiteral("*.exe"), QStringLiteral("*.dll"),
        QStringLiteral("*.js"), QStringLiteral("*.dat"),
    };
    PortalFilterRules rules;
    for (const QString &pattern : patterns)
        rules.append({0, pattern});
    return {
        {QCoreApplication::translate("PortalFilePicker", "Decompiler inputs"), rules},
        {QCoreApplication::translate("PortalFilePicker", "All files"), {{0, QStringLiteral("*")}}},
    };
}

PortalFileFilters metadataFilters()
{
    return {
        {QCoreApplication::translate("PortalFilePicker", "IL2CPP metadata"),
         {{0, QStringLiteral("global-metadata.dat")}, {0, QStringLiteral("*.dat")}}},
        {QCoreApplication::translate("PortalFilePicker", "All files"), {{0, QStringLiteral("*")}}},
    };
}

QString portalToken()
{
    return QStringLiteral("destruct_%1").arg(QRandomGenerator::global()->generate());
}

QByteArray portalPathBytes(const QString &path)
{
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}

// The portal wants a folder for the file chooser's starting point.
QString startFolder(const QUrl &suggested)
{
    const QFileInfo info(suggested.toLocalFile());
    if (info.isDir())
        return info.absoluteFilePath();
    if (info.isFile())
        return info.absolutePath();
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}
} // namespace

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent)
{
    registerPortalFilterTypes();
}

void PortalFilePicker::choose(Mode mode, const QUrl &suggested)
{
    const QString folder = startFolder(suggested);
    QVariantMap options;
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(folder));

    switch (mode) {
    case Input:
        options.insert(QStringLiteral("accept_label"),
                       QCoreApplication::translate("PortalFilePicker", "Open"));
        options.insert(QStringLiteral("multiple"), false);
        options.insert(QStringLiteral("filters"), QVariant::fromValue(inputFilters()));
        options.insert(QStringLiteral("current_filter"), QVariant::fromValue(inputFilters().first()));
        request(QStringLiteral("OpenFile"),
                QCoreApplication::translate("PortalFilePicker", "Open a file to decompile"),
                options, mode);
        return;
    case Metadata:
        options.insert(QStringLiteral("accept_label"),
                       QCoreApplication::translate("PortalFilePicker", "Open"));
        options.insert(QStringLiteral("multiple"), false);
        options.insert(QStringLiteral("filters"), QVariant::fromValue(metadataFilters()));
        options.insert(QStringLiteral("current_filter"), QVariant::fromValue(metadataFilters().first()));
        request(QStringLiteral("OpenFile"),
                QCoreApplication::translate("PortalFilePicker", "Choose global-metadata.dat"),
                options, mode);
        return;
    case Binary:
        options.insert(QStringLiteral("accept_label"),
                       QCoreApplication::translate("PortalFilePicker", "Choose"));
        options.insert(QStringLiteral("multiple"), false);
        options.insert(QStringLiteral("filters"),
                       QVariant::fromValue(PortalFileFilters{
                           {QCoreApplication::translate("PortalFilePicker", "All files"),
                            {{0, QStringLiteral("*")}}}}));
        request(QStringLiteral("OpenFile"),
                QCoreApplication::translate("PortalFilePicker", "Locate the destruct binary"),
                options, mode);
        return;
    case OutputDirectory:
        options.insert(QStringLiteral("accept_label"),
                       QCoreApplication::translate("PortalFilePicker", "Choose"));
        request(QStringLiteral("SelectFolder"),
                QCoreApplication::translate("PortalFilePicker", "Choose an output folder"),
                options, mode);
        return;
    case OffsetsDirectory:
        options.insert(QStringLiteral("accept_label"),
                       QCoreApplication::translate("PortalFilePicker", "Choose"));
        request(QStringLiteral("SelectFolder"),
                QCoreApplication::translate("PortalFilePicker",
                                            "Choose an IL2CPP field-offset dump"),
                options, mode);
        return;
    }
}

bool PortalFilePicker::connectToRequestPath(const QString &path)
{
    m_pendingPath = path;
    return QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
        this, SLOT(handleResponse(uint, QVariantMap)));
}

void PortalFilePicker::closePendingRequest()
{
    if (m_pendingPath.isEmpty())
        return;
    QDBusInterface request(QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                           QStringLiteral("org.freedesktop.portal.Request"),
                           QDBusConnection::sessionBus());
    request.asyncCall(QStringLiteral("Close"));
}

void PortalFilePicker::request(const QString &method, const QString &title,
                               QVariantMap options, Mode mode)
{
    // A request whose Response never arrives (the portal crashed or was
    // killed with the dialog up) must not wedge every later dialog behind
    // it: close the stuck one best-effort and replace it.
    if (m_hasPending) {
        closePendingRequest();
        clearPending();
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"),
                          bus);
    if (!portal.isValid()) {
        emit failed(QCoreApplication::translate(
            "PortalFilePicker", "The desktop file chooser is not available."));
        return;
    }

    // Subscribe to the Response signal at the request path the portal will
    // derive from our handle_token before making the call, so a response
    // cannot slip past while the match rule is still being installed.
    const QString token = portalToken();
    options.insert(QStringLiteral("handle_token"), token);
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString predictedPath =
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    m_pendingMode = mode;
    m_hasPending = true;
    if (!connectToRequestPath(predictedPath)) {
        clearPending();
        emit failed(QCoreApplication::translate(
            "PortalFilePicker", "Could not listen for the file chooser's response."));
        return;
    }

    const int generation = ++m_pendingGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(method, QString(), title, options), this);
    m_pendingWatcher = watcher;

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (m_pendingWatcher == watcher)
            m_pendingWatcher = nullptr;

        if (generation != m_pendingGeneration || !m_hasPending)
            return;

        if (reply.isError()) {
            clearPending();
            emit failed(QCoreApplication::translate(
                            "PortalFilePicker", "The file chooser failed: %1")
                            .arg(reply.error().message()));
            return;
        }

        // Old portal versions can hand back a different request path than the
        // handle_token predicts; move our subscription over if so.
        const QString actualPath = reply.value().path();
        if (actualPath != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(handleResponse(uint, QVariantMap)));
            if (!connectToRequestPath(actualPath)) {
                clearPending();
                emit failed(QCoreApplication::translate(
                    "PortalFilePicker", "Could not listen for the file chooser's response."));
            }
        }
    });
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results)
{
    const Mode mode = m_pendingMode;
    clearPending();

    if (response != 0)
        return;

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (uris.isEmpty())
        return;

    emit selected(static_cast<int>(mode), QUrl(uris.first()));
}

void PortalFilePicker::clearPending()
{
    if (m_pendingWatcher) {
        m_pendingWatcher->deleteLater();
        m_pendingWatcher = nullptr;
    }
    if (!m_pendingPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(handleResponse(uint, QVariantMap)));
    }
    m_pendingPath.clear();
    m_hasPending = false;
}
