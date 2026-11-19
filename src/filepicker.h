#pragma once

#include <QObject>
#include <QUrl>

// The one thing the window needs from the desktop: pick a file or a folder.
// It is abstract so tests can answer immediately instead of waiting for a
// portal dialog that no one is there to click.
class FilePicker : public QObject {
    Q_OBJECT

public:
    enum Mode {
        Input,
        Metadata,
        ElfMetadata,
        OutputDirectory,
        OffsetsDirectory,
        Binary,
    };
    Q_ENUM(Mode)

    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}

    // suggested is a file or folder to start the dialog at, or empty.
    virtual void choose(Mode mode, const QUrl &suggested = {}) = 0;

signals:
    void selected(int mode, const QUrl &url);
    void failed(const QString &message);
};
