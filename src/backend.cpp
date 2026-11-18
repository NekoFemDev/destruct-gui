#include "backend.h"

#include "destructsetup.h"
#include "filelistmodel.h"
#include "filepicker.h"
#include "logmodel.h"
#include "portalfilepicker.h"
#include "searchmodel.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {
constexpr qint64 previewLimit = 2 * 1024 * 1024;
constexpr qint64 searchFileLimit = 4 * 1024 * 1024;
constexpr int searchHitLimit = 400;

QString expandHome(const QString &path)
{
    if (path == QStringLiteral("~"))
        return QDir::homePath();
    if (path.startsWith(QStringLiteral("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

QString absolutePath(const QString &path)
{
    const QString expanded = expandHome(path.trimmed());
    if (expanded.isEmpty())
        return {};
    return QDir::cleanPath(QFileInfo(expanded).absoluteFilePath());
}
} // namespace

Backend::Backend(FilePicker *filePicker, QObject *parent)
    : QObject(parent),
      m_picker(filePicker ? filePicker : new PortalFilePicker(this)),
      m_setup(new DestructSetup({}, this)),
      m_files(new FileListModel(this)),
      m_logModel(new LogModel(this)),
      m_searchModel(new SearchModel(this))
{
    connectPicker();

    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, [this] {
        m_pendingLine += QString::fromUtf8(m_process.readAll());
        // A process that never emits a newline must not grow the buffer
        // forever; keep only the tail.
        if (m_pendingLine.size() > 1024 * 1024)
            m_pendingLine = m_pendingLine.right(64 * 1024);
        int newline;
        while ((newline = m_pendingLine.indexOf(QLatin1Char('\n'))) >= 0) {
            appendLog(m_pendingLine.left(newline));
            m_pendingLine.remove(0, newline + 1);
        }
    });
    connect(&m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) { finishJob(exitCode, status); });
    connect(&m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart)
                    failToStart(m_process.errorString());
            });

    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(4000);
    connect(&m_killTimer, &QTimer::timeout, this, [this] {
        if (m_state == Running)
            m_process.kill();
    });

    m_tick.setInterval(100);
    connect(&m_tick, &QTimer::timeout, this, [this] { emit elapsedChanged(); });

    m_noticeTimer.setSingleShot(true);
    m_noticeTimer.setInterval(5000);
    connect(&m_noticeTimer, &QTimer::timeout, this, [this] {
        if (!m_noticeText.isEmpty()) {
            m_noticeText.clear();
            emit noticeChanged();
        }
    });

    m_probeTimeout.setSingleShot(true);
    m_probeTimeout.setInterval(5000);
    connect(&m_probeTimeout, &QTimer::timeout, &m_probe, &QProcess::kill);
    connect(&m_probe, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_probeTimeout.stop();
        const QString output = QString::fromUtf8(m_probe.readAllStandardOutput()).trimmed();
        m_binaryProbing = false;
        // A random executable can answer `version`, but only DeStruct's own
        // output proves this is actually the decompiler the window drives.
        m_binaryReady = code == 0 && status == QProcess::NormalExit
            && output.contains(QStringLiteral("DeStruct"));
        m_binaryVersion.clear();
        if (m_binaryReady) {
            const QStringList lines = output.split(QLatin1Char('\n'));
            QString line = lines.value(0).trimmed();
            for (const QString &candidate : lines) {
                if (candidate.trimmed().startsWith(QStringLiteral("DeStruct"))) {
                    line = candidate.trimmed();
                    break;
                }
            }
            if (line.startsWith(QStringLiteral("DeStruct v")))
                line = line.mid(10);
            else if (line.startsWith(QStringLiteral("DeStruct ")))
                line = line.mid(9);
            m_binaryVersion = line;
        }
        emit binaryChanged();
        refreshValidation();
    });
    connect(&m_probe, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_probeTimeout.stop();
        m_binaryProbing = false;
        m_binaryReady = false;
        m_binaryVersion.clear();
        emit binaryChanged();
        refreshValidation();
    });

    connect(m_searchModel, &SearchModel::countChanged, this,
            &Backend::searchCountChanged);
    connect(m_setup, &DestructSetup::installed, this, [this](const QString &path) {
        setBinaryPath(path);
        showNotice(tr("DeStruct is ready."));
    });

    QSettings settings;
    m_recentFiles = settings.value(QStringLiteral("recentFiles")).toStringList();
    m_recentFiles.removeAll(QString());
    while (m_recentFiles.size() > 8)
        m_recentFiles.removeLast();

    discoverBinary();
    refreshValidation();
}

Backend::~Backend()
{
    m_killTimer.stop();
    m_tick.stop();
    m_probeTimeout.stop();
    if (m_searchCancel)
        m_searchCancel->store(true);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(1500)) {
            m_process.kill();
            m_process.waitForFinished(500);
        }
    }
    if (m_probe.state() != QProcess::NotRunning) {
        m_probe.kill();
        m_probe.waitForFinished(500);
    }
}

QString Backend::inputName() const
{
    return QFileInfo(m_inputPath).fileName();
}

QString Backend::inputDirectory() const
{
    return QFileInfo(m_inputPath).absolutePath();
}

QString Backend::formatKind() const
{
    if (m_inputPath.isEmpty())
        return m_metadataPath.isEmpty() ? QStringLiteral("none")
                                        : QStringLiteral("metadata");
    return formats::kindId(m_info.kind);
}

qint64 Backend::elapsedMs() const
{
    if (m_state == Running && m_elapsed.isValid())
        return m_elapsed.elapsed();
    return m_finalElapsedMs;
}

QObject *Backend::files() const
{
    return m_files;
}

QObject *Backend::setup() const
{
    return m_setup;
}

QObject *Backend::log() const
{
    return m_logModel;
}

QObject *Backend::searchResults() const
{
    return m_searchModel;
}

int Backend::fileCount() const
{
    return m_files->totalCount();
}

qint64 Backend::resultBytes() const
{
    return m_files->totalSize();
}

QString Backend::previewName() const
{
    return QFileInfo(m_previewRelativePath).fileName();
}

QString Backend::previewFolder() const
{
    const QString folder = QFileInfo(m_previewRelativePath).path();
    return folder == QStringLiteral(".") ? QString() : folder;
}

int Backend::searchCount() const
{
    return m_searchModel->count();
}

QStringList Backend::arguments() const
{
    formats::JobOptions options;
    options.command = m_command;
    options.input = m_inputPath;
    options.metadata = m_metadataPath;
    options.output = m_outputPath;
    options.offsets = m_offsetsPath;
    options.decompile = m_decompile;
    options.splitFunctions = m_splitFunctions;
    options.crossReferences = m_crossReferences;
    options.simplifyCfg = m_simplifyCfg;
    options.emitLLVM = m_emitLLVM;
    options.regsAsVars = m_regsAsVars;
    options.noEnhance = m_noEnhance;
    options.noLocationComments = m_noLocationComments;
    options.deobfuscate = m_deobfuscate;
    options.verbose = m_verbose;
    options.project = true;
    return formats::arguments(options);
}

QString Backend::commandLine() const
{
    formats::JobOptions options;
    options.command = m_command;
    options.input = m_inputPath;
    options.metadata = m_metadataPath;
    options.output = m_outputPath;
    options.offsets = m_offsetsPath;
    options.decompile = m_decompile;
    options.splitFunctions = m_splitFunctions;
    options.crossReferences = m_crossReferences;
    options.simplifyCfg = m_simplifyCfg;
    options.emitLLVM = m_emitLLVM;
    options.regsAsVars = m_regsAsVars;
    options.noEnhance = m_noEnhance;
    options.noLocationComments = m_noLocationComments;
    options.deobfuscate = m_deobfuscate;
    options.verbose = m_verbose;
    options.project = true;
    return formats::commandLine(options);
}

void Backend::connectPicker()
{
    connect(m_picker, &FilePicker::selected, this, [this](int mode, const QUrl &url) {
        if (!url.isLocalFile())
            return;
        handlePickedPath(mode, url.toLocalFile());
    });
    connect(m_picker, &FilePicker::failed, this, &Backend::showNotice);
}

void Backend::handlePickedPath(int mode, const QString &path)
{
    switch (static_cast<FilePicker::Mode>(mode)) {
    case FilePicker::Input: {
        const formats::Info info = formats::detect(absolutePath(path));
        if (info.kind == formats::Kind::Metadata)
            setMetadataPath(path);
        else
            setInputPath(path);
        return;
    }
    case FilePicker::Metadata:
        setMetadataPath(path);
        return;
    case FilePicker::OutputDirectory:
        setOutputPath(path);
        return;
    case FilePicker::OffsetsDirectory:
        setOffsetsPath(path);
        return;
    case FilePicker::Binary:
        setBinaryPath(path);
        return;
    }
}

void Backend::chooseInput()
{
    m_picker->choose(FilePicker::Input,
                     m_inputPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_inputPath));
}

void Backend::chooseMetadata()
{
    m_picker->choose(FilePicker::Metadata,
                     m_metadataPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_metadataPath));
}

void Backend::chooseOutput()
{
    m_picker->choose(FilePicker::OutputDirectory, QUrl::fromLocalFile(m_outputPath));
}

void Backend::chooseOffsets()
{
    m_picker->choose(FilePicker::OffsetsDirectory,
                     m_offsetsPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_offsetsPath));
}

void Backend::chooseBinary()
{
    m_picker->choose(FilePicker::Binary,
                     m_binaryPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_binaryPath));
}

void Backend::acceptDrop(const QUrl &url)
{
    if (!url.isLocalFile())
        return;
    if (running()) {
        showNotice(tr("Wait for the current job to finish, or cancel it first."));
        return;
    }
    const QString path = url.toLocalFile();
    const QFileInfo info(path);
    if (info.isDir()) {
        setOutputPath(path);
        return;
    }
    const formats::Info detected = formats::detect(path);
    if (detected.kind == formats::Kind::Metadata)
        setMetadataPath(path);
    else
        setInputPath(path);
}

void Backend::setInputPath(const QString &path)
{
    if (running())
        return;
    const QString absolute = absolutePath(path);
    if (absolute.isEmpty())
        return;

    const formats::Info info = formats::detect(absolute);
    if (info.kind == formats::Kind::Metadata) {
        setMetadataPath(absolute);
        return;
    }

    const bool changed = absolute != m_inputPath;
    m_inputPath = absolute;
    m_info = info;
    m_inputSize = QFileInfo(absolute).size();

    if (changed)
        applyDefaultOutput();

    updateAvailableCommands();
    pickCommandForInput();

    m_errorText.clear();
    m_finalElapsedMs = 0;
    m_pendingLine.clear();
    resetResults();
    if (m_state != Running) {
        m_state = Idle;
        emit stateChanged();
    }
    emit inputChanged();
    emit outputChanged();
    emit commandChanged();
    emit commandsChanged();
    emit optionsChanged();
    refreshValidation();
}

void Backend::setMetadataPath(const QString &path)
{
    if (running())
        return;
    const QString absolute = path.isEmpty() ? QString() : absolutePath(path);
    if (absolute == m_metadataPath)
        return;

    m_metadataPath = absolute;
    emit metadataChanged();
    updateAvailableCommands();

    // Metadata has no meaning to any command but il2cpp, so choosing it is
    // also a request to switch there. If no library is chosen yet the command
    // still shows, with a clear "choose libil2cpp.so" state.
    if (!m_metadataPath.isEmpty() && m_availableCommands.contains(QStringLiteral("il2cpp"))
        && m_command != QStringLiteral("il2cpp")) {
        setCommand(QStringLiteral("il2cpp"));
    } else if (m_command.isEmpty() && m_availableCommands.size() == 1) {
        setCommand(m_availableCommands.first());
    } else {
        if (m_state != Running && m_files->totalCount() > 0) {
            resetResults();
            m_state = Idle;
            emit stateChanged();
        }
        refreshValidation();
    }
}

void Backend::setOutputPath(const QString &path)
{
    if (running() || path.trimmed().isEmpty())
        return;
    const QString absolute = absolutePath(path);
    if (absolute.isEmpty() || absolute == m_outputPath)
        return;
    m_outputPath = absolute;
    emit outputChanged();
    if (m_state != Running && m_files->totalCount() > 0) {
        resetResults();
        m_state = Idle;
        emit stateChanged();
    }
    refreshValidation();
}

void Backend::setOffsetsPath(const QString &path)
{
    if (running())
        return;
    const QString absolute = path.trimmed().isEmpty() ? QString() : absolutePath(path);
    if (absolute == m_offsetsPath)
        return;
    m_offsetsPath = absolute;
    emit offsetsChanged();
    refreshValidation();
}

void Backend::applyDefaultOutput()
{
    m_outputPath = formats::defaultOutputPath(m_inputPath);
}

void Backend::discoverBinary()
{
    QSettings settings;
    const QString configured = settings.value(QStringLiteral("binaryPath")).toString();
    QStringList candidates;
    if (!configured.isEmpty())
        candidates << configured;
    // Deliberately no developer-specific checkout paths: the system
    // location comes first, then whatever the user's PATH already resolves.
    candidates << QStringLiteral("/usr/bin/destruct");
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("destruct"));
    if (!onPath.isEmpty())
        candidates << onPath;
    candidates << QStringLiteral("/usr/local/bin/destruct");
    // A copy built by the setup dialog lives under the app's data folder.
    // Finding it here makes that build survive lost settings and later runs.
    candidates << QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                      .filePath(QStringLiteral("DeStruct/destruct"));
    candidates.removeDuplicates();

    for (const QString &candidate : candidates) {
        const QFileInfo info(expandHome(candidate));
        if (info.isFile() && info.isExecutable()) {
            m_binaryPath = info.absoluteFilePath();
            break;
        }
    }
    if (m_binaryPath.isEmpty() && !configured.isEmpty())
        m_binaryPath = expandHome(configured);
    probeBinary();
}

void Backend::setBinaryPath(const QString &path)
{
    if (running())
        return;
    const QString absolute = absolutePath(path);
    if (absolute.isEmpty())
        return;
    const QFileInfo info(absolute);
    if (!info.isFile() || !info.isExecutable()) {
        showNotice(tr("That file is not executable. Choose the destruct binary itself."));
        return;
    }
    m_binaryPath = absolute;
    QSettings settings;
    settings.setValue(QStringLiteral("binaryPath"), absolute);
    emit binaryChanged();
    probeBinary();
}

void Backend::probeBinary()
{
    if (m_probe.state() != QProcess::NotRunning) {
        m_probe.kill();
        m_probe.waitForFinished(500);
    }
    const QFileInfo info(m_binaryPath);
    if (m_binaryPath.isEmpty() || !info.isFile() || !info.isExecutable()) {
        m_binaryProbing = false;
        m_binaryReady = false;
        m_binaryVersion.clear();
        emit binaryChanged();
        refreshValidation();
        return;
    }

    m_binaryProbing = true;
    m_binaryReady = false;
    emit binaryChanged();
    m_probe.setProgram(m_binaryPath);
    m_probe.setArguments({QStringLiteral("version")});
    m_probe.start();
    m_probeTimeout.start();
}

QStringList Backend::commandsFor(const formats::Info &info)
{
    switch (info.kind) {
    case formats::Kind::JVM:
        return {QStringLiteral("jvm")};
    case formats::Kind::Dex:
        return {QStringLiteral("dex")};
    case formats::Kind::Apk: {
        QStringList commands;
        if (info.hasLibapp && !info.hasClassesDex)
            commands << QStringLiteral("flutter");
        commands << QStringLiteral("dex");
        if (info.hasLibapp && info.hasClassesDex)
            commands << QStringLiteral("flutter");
        return commands;
    }
    case formats::Kind::Hermes:
        return {QStringLiteral("hermes")};
    case formats::Kind::JavaScript:
        return {QStringLiteral("js")};
    case formats::Kind::Elf:
        if (info.flutterLibrary)
            return {QStringLiteral("flutter"), QStringLiteral("elf"),
                    QStringLiteral("il2cpp")};
        return {QStringLiteral("elf"), QStringLiteral("il2cpp")};
    case formats::Kind::Pe:
        return {QStringLiteral("pe")};
    case formats::Kind::Metadata:
        return {QStringLiteral("il2cpp")};
    case formats::Kind::Unknown:
        return {};
    }
    return {};
}

void Backend::updateAvailableCommands()
{
    if (m_inputPath.isEmpty()) {
        m_availableCommands = m_metadataPath.isEmpty()
            ? QStringList()
            : QStringList{QStringLiteral("il2cpp")};
        return;
    }
    m_availableCommands = commandsFor(m_info);
}

void Backend::pickCommandForInput()
{
    QString wanted;
    if (!m_metadataPath.isEmpty() && m_availableCommands.contains(QStringLiteral("il2cpp")))
        wanted = QStringLiteral("il2cpp");
    else if (!m_availableCommands.isEmpty())
        wanted = m_availableCommands.first();

    if (wanted == m_command)
        return;
    m_command = wanted;
    resetOptions();
}

void Backend::resetOptions()
{
    const bool elf = m_command == QStringLiteral("elf");
    m_decompile = elf && m_info.aarch64;
    m_splitFunctions = false;
    m_crossReferences = false;
    m_simplifyCfg = false;
    m_emitLLVM = false;
    m_regsAsVars = false;
    m_noEnhance = false;
    m_noLocationComments = false;
    m_deobfuscate = false;
    emit optionsChanged();
}

void Backend::setCommand(const QString &command)
{
    if (running() || command == m_command)
        return;
    if (!command.isEmpty() && !m_availableCommands.contains(command))
        return;
    m_command = command;
    resetOptions();
    emit commandChanged();
    if (m_state != Running && m_files->totalCount() > 0) {
        resetResults();
        m_state = Idle;
        emit stateChanged();
    }
    refreshValidation();
}

void Backend::setDecompile(bool value)
{
    value = value && m_command == QStringLiteral("elf") && m_info.aarch64;
    if (value == m_decompile)
        return;
    m_decompile = value;
    if (!value) {
        m_splitFunctions = false;
        m_crossReferences = false;
        m_simplifyCfg = false;
        m_emitLLVM = false;
        m_regsAsVars = false;
    }
    emit optionsChanged();
    refreshValidation();
}

void Backend::setSplitFunctions(bool value)
{
    if (value && m_command != QStringLiteral("elf"))
        return;
    if (value && !m_decompile)
        m_decompile = true;
    if (value == m_splitFunctions)
        return;
    m_splitFunctions = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setCrossReferences(bool value)
{
    if (value && m_command != QStringLiteral("elf"))
        return;
    if (value && !m_decompile)
        m_decompile = true;
    if (value == m_crossReferences)
        return;
    m_crossReferences = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setSimplifyCfg(bool value)
{
    if (value && m_command != QStringLiteral("elf"))
        return;
    if (value && !m_decompile)
        m_decompile = true;
    if (value == m_simplifyCfg)
        return;
    m_simplifyCfg = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setEmitLLVM(bool value)
{
    if (value && m_command != QStringLiteral("elf"))
        return;
    if (value && !m_decompile)
        m_decompile = true;
    if (value == m_emitLLVM)
        return;
    m_emitLLVM = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setRegsAsVars(bool value)
{
    if (value && m_command != QStringLiteral("elf"))
        return;
    if (value && !m_decompile)
        m_decompile = true;
    if (value == m_regsAsVars)
        return;
    m_regsAsVars = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setNoEnhance(bool value)
{
    if (value && m_command != QStringLiteral("elf"))
        return;
    if (value == m_noEnhance)
        return;
    m_noEnhance = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setNoLocationComments(bool value)
{
    if (value && m_command != QStringLiteral("elf"))
        return;
    if (value == m_noLocationComments)
        return;
    m_noLocationComments = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setDeobfuscate(bool value)
{
    if (value
        && m_command != QStringLiteral("jvm")
        && m_command != QStringLiteral("dex")
        && m_command != QStringLiteral("flutter")
        && m_command != QStringLiteral("pe"))
        return;
    if (value == m_deobfuscate)
        return;
    m_deobfuscate = value;
    emit optionsChanged();
    refreshValidation();
}

void Backend::setVerbose(bool value)
{
    if (value == m_verbose)
        return;
    m_verbose = value;
    emit optionsChanged();
}

void Backend::refreshValidation()
{
    QString code;
    QString detail;

    if (!running()) {
        if (m_inputPath.isEmpty()) {
            code = m_metadataPath.isEmpty() ? QStringLiteral("noInput")
                                            : QStringLiteral("missingLibrary");
        } else if (!QFileInfo(m_inputPath).isFile()) {
            code = QStringLiteral("inputMissing");
            detail = m_inputPath;
        } else if (!m_info.signatureValid) {
            code = QStringLiteral("badSignature");
            detail = formatKind();
        } else if (m_command.isEmpty()) {
            code = QStringLiteral("noCommand");
        } else if (m_command == QStringLiteral("il2cpp") && m_metadataPath.isEmpty()) {
            code = QStringLiteral("missingMetadata");
        } else if (m_command == QStringLiteral("il2cpp")
                   && [this] {
                       const formats::Info metadata = formats::detect(m_metadataPath);
                       return !QFileInfo(m_metadataPath).isFile()
                           || metadata.kind != formats::Kind::Metadata
                           || !metadata.signatureValid;
                   }()) {
            code = QStringLiteral("metadataMissing");
            detail = m_metadataPath;
        } else if (m_command == QStringLiteral("elf") && m_decompile && !m_info.aarch64) {
            code = QStringLiteral("notAarch64");
            detail = m_info.architecture;
        } else if (m_command == QStringLiteral("elf") && m_decompile
                   && !m_offsetsPath.isEmpty() && !QFileInfo::exists(m_offsetsPath)) {
            code = QStringLiteral("offsetsMissing");
            detail = m_offsetsPath;
        } else if (m_outputPath.isEmpty()) {
            code = QStringLiteral("outputMissing");
        } else if (!outputWritable()) {
            code = QFileInfo(m_outputPath).isFile() ? QStringLiteral("outputIsFile")
                                                    : QStringLiteral("outputNotWritable");
            detail = m_outputPath;
        } else if (!m_binaryReady) {
            code = m_binaryProbing ? QStringLiteral("probingBinary")
                                   : QStringLiteral("missingBinary");
            detail = m_binaryPath;
        }
    }

    setValidation(code, detail);
}

void Backend::setValidation(const QString &code, const QString &detail)
{
    if (code == m_validationCode && detail == m_validationDetail)
        return;
    m_validationCode = code;
    m_validationDetail = detail;
    emit validationChanged();
}

bool Backend::outputWritable() const
{
    if (m_outputPath.isEmpty())
        return false;
    return pathWritable(m_outputPath);
}

bool Backend::pathWritable(const QString &path)
{
    const QFileInfo info(path);
    if (info.exists())
        return info.isDir() && info.isWritable();
    QString parent = info.absolutePath();
    while (!QFileInfo::exists(parent) && parent != QStringLiteral("/"))
        parent = QFileInfo(parent).absolutePath();
    const QFileInfo existing(parent);
    return existing.isDir() && existing.isWritable();
}

void Backend::run()
{
    refreshValidation();
    if (!canRun())
        return;

    m_errorText.clear();
    m_lastErrorLine.clear();
    m_pendingLine.clear();
    m_cancelling = false;
    m_finalElapsedMs = 0;
    m_logModel->clear();
    resetResults();

    if (!QDir().mkpath(m_outputPath)) {
        m_errorText = tr("Could not create the output folder: %1").arg(m_outputPath);
        appendLog(QStringLiteral("Error: ") + m_errorText);
        m_state = Failed;
        emit stateChanged();
        refreshValidation();
        return;
    }

    appendLog(QStringLiteral("$ ") + commandLine());
    m_state = Running;
    m_elapsed.start();
    m_tick.start();
    emit stateChanged();
    refreshValidation();

    m_process.setProgram(m_binaryPath);
    m_process.setArguments(arguments());
    m_process.setWorkingDirectory(QFileInfo(m_inputPath).absolutePath());
    m_process.start();
}

void Backend::cancel()
{
    if (!running() || m_cancelling)
        return;
    m_cancelling = true;
    appendLog(tr("Stopping..."));
    emit stateChanged();
    m_process.terminate();
    m_killTimer.start();
}

void Backend::appendLog(const QString &line)
{
    m_logModel->append(line);
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
        m_activity = trimmed;
        emit activityChanged();
    }
    if (trimmed.startsWith(QStringLiteral("Error:"))
        || trimmed.startsWith(QStringLiteral("panic:")))
        m_lastErrorLine = trimmed;
}

void Backend::flushPendingLine()
{
    if (m_pendingLine.isEmpty())
        return;
    appendLog(m_pendingLine);
    m_pendingLine.clear();
}

QString Backend::trimmedError(const QString &line)
{
    QString text = line.trimmed();
    if (text.startsWith(QStringLiteral("Error:")))
        text = text.mid(6).trimmed();
    return text;
}

void Backend::finishJob(int exitCode, QProcess::ExitStatus status)
{
    if (m_state != Running)
        return;
    m_killTimer.stop();
    m_tick.stop();
    m_pendingLine += QString::fromUtf8(m_process.readAll());
    flushPendingLine();
    m_finalElapsedMs = m_elapsed.isValid() ? m_elapsed.elapsed() : 0;
    emit elapsedChanged();

    const bool success = exitCode == 0 && status == QProcess::NormalExit;
    const bool wasCancelling = m_cancelling;
    m_cancelling = false;

    if (wasCancelling) {
        appendLog(tr("Cancelled."));
        m_state = Cancelled;
        emit stateChanged();
        refreshValidation();
        return;
    }

    if (success) {
        scanResults();
        m_state = Succeeded;
        addRecent(m_inputPath);
    } else {
        QString error = m_lastErrorLine;
        if (error.isEmpty()) {
            error = tr("destruct exited with code %1.").arg(exitCode);
            if (status == QProcess::CrashExit)
                error = tr("destruct stopped unexpectedly.");
        }
        m_errorText = trimmedError(error);
        m_state = Failed;
    }
    emit stateChanged();
    refreshValidation();
}

void Backend::failToStart(const QString &message)
{
    if (m_state != Running)
        return;
    m_killTimer.stop();
    m_tick.stop();
    m_finalElapsedMs = 0;
    emit elapsedChanged();
    m_errorText = tr("Could not start destruct: %1").arg(message);
    appendLog(QStringLiteral("Error: ") + m_errorText);
    m_state = Failed;
    emit stateChanged();
    refreshValidation();
}

void Backend::showNotice(const QString &text)
{
    if (text.isEmpty())
        return;
    m_noticeText = text;
    emit noticeChanged();
    m_noticeTimer.start();
}

void Backend::dismissNotice()
{
    m_noticeTimer.stop();
    if (m_noticeText.isEmpty())
        return;
    m_noticeText.clear();
    emit noticeChanged();
}

void Backend::resetResults()
{
    m_files->clear();
    m_searchGeneration++;
    if (m_searchCancel)
        m_searchCancel->store(true);
    m_searchCancel.reset();
    m_searching = false;
    m_searchQuery.clear();
    m_searchModel->clear();
    m_primaryRelativePath.clear();
    m_collectionResult = false;
    m_previewRelativePath.clear();
    m_previewText.clear();
    m_previewSize = 0;
    m_previewTextual = true;
    m_previewTruncated = false;
    m_previewLine = 0;
    m_errorText.clear();
    emit searchingChanged();
    emit searchQueryChanged();
    emit resultsChanged();
    emit previewChanged();
}

void Backend::scanResults()
{
    m_files->scan(m_outputPath);
    m_primaryRelativePath = findPrimaryArtifact();
    m_collectionResult = m_command == QStringLiteral("jvm")
        || m_command == QStringLiteral("dex");
    emit resultsChanged();
}

QString Backend::findPrimaryArtifact() const
{
    // Most preferred first. The enhanced C is the final ELF artifact when
    // enhancements ran, so the preview should open it rather than the raw
    // lift next to it.
    QStringList suffixes;
    QString exact;
    if (m_command == QStringLiteral("elf")) {
        if (m_splitFunctions)
            suffixes << QStringLiteral("c");
        else
            suffixes << QStringLiteral("decompiled.c.enhanced")
                     << QStringLiteral("decompiled.c");
    } else if (m_command == QStringLiteral("hermes")) {
        suffixes << QStringLiteral("hasm");
    } else if (m_command == QStringLiteral("flutter")) {
        exact = QStringLiteral("asm.txt");
    } else if (m_command == QStringLiteral("il2cpp")) {
        exact = QStringLiteral("dump.cs");
    } else if (m_command == QStringLiteral("js")) {
        suffixes << QStringLiteral("deobf.js");
    } else {
        return {};
    }

    QStringList all;
    QDirIterator iterator(m_outputPath, QDir::Files | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext())
        all << iterator.next();

    if (!exact.isEmpty()) {
        for (const QString &path : all) {
            if (QFileInfo(path).fileName() == exact)
                return QDir(m_outputPath).relativeFilePath(path);
        }
        return {};
    }

    for (const QString &suffix : suffixes) {
        QStringList matches;
        for (const QString &path : all) {
            if (path.toLower().endsWith(QLatin1Char('.') + suffix))
                matches << path;
        }
        if (matches.isEmpty())
            continue;
        std::sort(matches.begin(), matches.end());
        return QDir(m_outputPath).relativeFilePath(matches.first());
    }
    return {};
}

QString Backend::resolveInOutput(const QString &relativePath) const
{
    if (m_outputPath.isEmpty() || relativePath.isEmpty())
        return {};
    const QString root = QFileInfo(m_outputPath).canonicalFilePath();
    if (root.isEmpty())
        return {};
    const QString candidate = QFileInfo(QDir(m_outputPath).filePath(relativePath))
                                  .canonicalFilePath();
    if (candidate.isEmpty())
        return {};
    if (candidate != root
        && !candidate.startsWith(root + QDir::separator()))
        return {};
    return candidate;
}

void Backend::openPreview(const QString &relativePath, int line)
{
    const QString absolute = resolveInOutput(relativePath);
    if (absolute.isEmpty())
        return;
    const QFileInfo info(absolute);
    if (!info.isFile())
        return;

    QFile file(absolute);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QByteArray data = file.read(previewLimit);
    const bool textual = !data.left(8192).contains('\0');

    m_previewRelativePath = QDir(m_outputPath).relativeFilePath(absolute);
    m_previewSize = info.size();
    m_previewTextual = textual;
    m_previewTruncated = info.size() > previewLimit;
    m_previewText = textual ? QString::fromUtf8(data) : QString();
    m_previewLine = line;
    emit previewChanged();
}

void Backend::openPreviewExternally()
{
    const QString absolute = resolveInOutput(m_previewRelativePath);
    if (!absolute.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(absolute));
}

void Backend::openOutputFolder()
{
    if (m_outputPath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputPath));
}

void Backend::copyLog()
{
    QGuiApplication::clipboard()->setText(m_logModel->toText());
    showNotice(tr("Log copied to the clipboard."));
}

void Backend::copyCommand()
{
    QGuiApplication::clipboard()->setText(commandLine());
    showNotice(tr("Command copied to the clipboard."));
}

void Backend::filterFiles(const QString &filter)
{
    m_files->setFilter(filter);
}

void Backend::searchOutput(const QString &query, bool caseSensitive)
{
    const QString needle = query.trimmed();
    if (needle.size() < 2) {
        clearSearch();
        return;
    }
    if (m_outputPath.isEmpty())
        return;

    if (m_searchCancel)
        m_searchCancel->store(true);
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    m_searchCancel = cancelled;
    const int generation = ++m_searchGeneration;

    m_searchQuery = needle;
    m_searching = true;
    m_searchModel->clear();
    emit searchQueryChanged();
    emit searchingChanged();

    const QString root = m_outputPath;
    const Qt::CaseSensitivity sensitivity = caseSensitive ? Qt::CaseSensitive
                                                           : Qt::CaseInsensitive;
    auto *watcher = new QFutureWatcher<QVector<SearchModel::Hit>>(this);
    connect(watcher, &QFutureWatcher<QVector<SearchModel::Hit>>::finished, this,
            [this, watcher, generation] {
                const QVector<SearchModel::Hit> hits = watcher->result();
                watcher->deleteLater();
                if (generation != m_searchGeneration)
                    return;
                m_searching = false;
                m_searchModel->setHits(hits);
                emit searchingChanged();
            });

    const QFuture<QVector<SearchModel::Hit>> future = QtConcurrent::run(
        [root, needle, sensitivity, cancelled]() {
            QVector<SearchModel::Hit> hits;
            const QDir directory(root);
            QDirIterator iterator(root, QDir::Files | QDir::NoSymLinks,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                if (cancelled->load())
                    return hits;
                const QString path = iterator.next();
                if (!formats::isTextPath(path))
                    continue;
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly) || file.size() > searchFileLimit)
                    continue;
                const QByteArray data = file.readAll();
                if (data.left(8192).contains('\0'))
                    continue;
                const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
                for (int i = 0; i < lines.size(); ++i) {
                    if ((i & 127) == 0 && cancelled->load())
                        return hits;
                    const QString &line = lines.at(i);
                    const int match = line.indexOf(needle, 0, sensitivity);
                    if (match < 0)
                        continue;
                    QString snippet = line.trimmed();
                    if (snippet.size() > 280) {
                        int start = qMax(0, match - 100);
                        snippet = snippet.mid(start, 260).trimmed();
                        if (start > 0)
                            snippet.prepend(QChar(0x2026));
                        snippet.append(QChar(0x2026));
                    }
                    hits.append({directory.relativeFilePath(path), i + 1, snippet});
                    if (hits.size() >= searchHitLimit)
                        return hits;
                }
            }
            return hits;
        });
    watcher->setFuture(future);
}

void Backend::clearSearch()
{
    m_searchGeneration++;
    if (m_searchCancel)
        m_searchCancel->store(true);
    m_searchCancel.reset();
    const bool wasSearching = m_searching;
    m_searching = false;
    const bool hadQuery = !m_searchQuery.isEmpty();
    m_searchQuery.clear();
    m_searchModel->clear();
    if (wasSearching)
        emit searchingChanged();
    if (hadQuery)
        emit searchQueryChanged();
}

void Backend::addRecent(const QString &path)
{
    if (path.isEmpty())
        return;
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 8)
        m_recentFiles.removeLast();
    saveRecent();
    emit recentFilesChanged();
}

void Backend::saveRecent()
{
    QSettings settings;
    settings.setValue(QStringLiteral("recentFiles"), m_recentFiles);
}

void Backend::useRecent(const QString &path)
{
    if (running())
        return;
    if (!QFileInfo(path).isFile()) {
        m_recentFiles.removeAll(path);
        saveRecent();
        emit recentFilesChanged();
        showNotice(tr("That file is no longer there."));
        return;
    }
    setInputPath(path);
}

void Backend::clearRecent()
{
    m_recentFiles.clear();
    saveRecent();
    emit recentFilesChanged();
}
