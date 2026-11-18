#pragma once

#include <QDBusArgument>
#include <QVariantMap>

#include "filepicker.h"

class QDBusPendingCallWatcher;

class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void choose(Mode mode, const QUrl &suggested = {}) override;

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    void request(const QString &method, const QString &title, QVariantMap options, Mode mode);
    bool connectToRequestPath(const QString &path);
    void clearPending();
    // Best-effort Close of the pending portal request, if there is one.
    void closePendingRequest();

    QString m_pendingPath;
    Mode m_pendingMode = Input;
    bool m_hasPending = false;
    // Bumped per request, so a late reply for a replaced request is ignored.
    int m_pendingGeneration = 0;
    QDBusPendingCallWatcher *m_pendingWatcher = nullptr;
};
