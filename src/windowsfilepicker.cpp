#include "windowsfilepicker.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

WindowsFilePicker::WindowsFilePicker(QObject *parent) : FilePicker(parent) {}

void WindowsFilePicker::choose(Mode mode, const QUrl &suggested)
{
    QString start = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (suggested.isLocalFile()) {
        const QFileInfo info(suggested.toLocalFile());
        if (info.isDir())
            start = info.absoluteFilePath();
        else
            start = info.absolutePath();
    }

    const QString allFiles = tr("All files (*.*)");
    QString path;
    switch (mode) {
    case Input:
        path = QFileDialog::getOpenFileName(
            nullptr, tr("Open a file to decompile"), start,
            tr("Decompiler inputs (*.jar *.class *.dex *.apk *.hbc *.bundle *.so *.elf *.exe *.dll *.js *.dat)")
                + QStringLiteral(";;") + allFiles);
        break;
    case Metadata:
    case ElfMetadata:
        path = QFileDialog::getOpenFileName(
            nullptr, tr("Choose global-metadata.dat"), start,
            tr("IL2CPP metadata (global-metadata.dat *.dat)")
                + QStringLiteral(";;") + allFiles);
        break;
    case OutputDirectory:
        path = QFileDialog::getExistingDirectory(
            nullptr, tr("Choose an output folder"), start);
        break;
    case OffsetsDirectory:
        path = QFileDialog::getExistingDirectory(
            nullptr, tr("Choose an IL2CPP field-offset dump"), start);
        break;
    case Binary:
        path = QFileDialog::getOpenFileName(
            nullptr, tr("Locate the destruct binary"), start,
            tr("Executables (*.exe)") + QStringLiteral(";;") + allFiles);
        break;
    }

    if (!path.isEmpty())
        emit selected(static_cast<int>(mode), QUrl::fromLocalFile(path));
}
