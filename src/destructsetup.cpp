#include "destructsetup.h"

#include "logmodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace {
constexpr int setupLogLimit = 1024 * 1024;

QString quoted(const QString &value)
{
    if (value.isEmpty())
        return QStringLiteral("''");
    if (!value.contains(QLatin1Char(' ')))
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}
} // namespace

DestructSetup::DestructSetup(const QString &dataDirectory, QObject *parent)
    : QObject(parent),
      m_log(new LogModel(this))
{
    m_dataDir = dataDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : QDir::cleanPath(QFileInfo(dataDirectory).absoluteFilePath());
    m_sourceDir = QDir(m_dataDir).filePath(QStringLiteral("DeStruct"));
    m_tempDir = QDir(m_dataDir).filePath(QStringLiteral("DeStruct-build"));
    m_archivePath = QDir(m_dataDir).filePath(QStringLiteral("DeStruct-main.tar.gz"));

    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, [this] { readOutput(); });
    connect(&m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                handleFinished(exitCode, exitStatus);
            });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        handleError(error);
    });

    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(5000);
    connect(&m_killTimer, &QTimer::timeout, this, [this] {
        if (m_process.state() != QProcess::NotRunning)
            m_process.kill();
    });
}

DestructSetup::~DestructSetup()
{
    m_killTimer.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(1500)) {
            m_process.kill();
            m_process.waitForFinished(500);
        }
    }
}

QString DestructSetup::binaryPath() const
{
    return QDir(m_sourceDir).filePath(QStringLiteral("destruct"));
}

QObject *DestructSetup::log() const
{
    return m_log;
}

QString DestructSetup::downloadUrl()
{
    return QStringLiteral(
        "https://github.com/NekoFemDev/DeStruct/archive/refs/heads/main.tar.gz");
}

QStringList DestructSetup::missingTools()
{
    QStringList missing;
    for (const QString &tool : {QStringLiteral("curl"), QStringLiteral("tar"),
                                QStringLiteral("make"), QStringLiteral("go")}) {
        if (QStandardPaths::findExecutable(tool).isEmpty())
            missing << tool;
    }

    // The Capstone build and cgo both need a C compiler. Prefer the plain
    // names toolchains provide, and pass the resolved one to make below.
    m_compiler.clear();
    for (const QString &compiler : {QStringLiteral("cc"), QStringLiteral("gcc"),
                                    QStringLiteral("clang")}) {
        const QString path = QStandardPaths::findExecutable(compiler);
        if (!path.isEmpty()) {
            m_compiler = path;
            break;
        }
    }
    if (m_compiler.isEmpty())
        missing << QStringLiteral("cc");
    return missing;
}

QString DestructSetup::missingToolsMessage(const QStringList &missing) const
{
    QStringList packages;
    for (const QString &tool : missing) {
        if (tool == QStringLiteral("cc"))
            packages << QStringLiteral("gcc");
        else
            packages << tool;
    }
    packages.removeDuplicates();
    return tr("Missing tools: %1. Install them with omarchy-pkg-add %2, then try again.")
        .arg(missing.join(QStringLiteral(", ")), packages.join(QLatin1Char(' ')));
}

void DestructSetup::install()
{
    if (m_running)
        return;

    m_error.clear();
    m_succeeded = false;
    m_cancelling = false;
    m_pendingLine.clear();
    m_lastLine.clear();
    m_step = Step::Idle;
    m_log->clear();
    setStatus(tr("Checking prerequisites..."));
    m_running = true;
    emit stateChanged();
    appendLog(tr("Downloading DeStruct from GitHub and building destruct."));

    const QStringList missing = missingTools();
    if (!missing.isEmpty()) {
        fail(missingToolsMessage(missing));
        return;
    }

    if (m_dataDir.isEmpty()) {
        fail(tr("Could not find a writable application data folder."));
        return;
    }
    if (!QDir().mkpath(m_dataDir)) {
        fail(tr("Could not create %1.").arg(m_dataDir));
        return;
    }

    removeTemp();
    QFile::remove(m_archivePath);
    startDownload();
}

void DestructSetup::startDownload()
{
    setStatus(tr("Downloading the DeStruct source..."));
    appendLog(QStringLiteral("$ curl -fL --retry 3 --no-progress-meter -o %1 %2")
                  .arg(quoted(m_archivePath), downloadUrl()));
    startProcess(Step::Download, QStringLiteral("curl"),
                 {QStringLiteral("-fL"), QStringLiteral("--retry"), QStringLiteral("3"),
                  QStringLiteral("--connect-timeout"), QStringLiteral("20"),
                  QStringLiteral("--no-progress-meter"), QStringLiteral("-o"),
                  m_archivePath, downloadUrl()});
}

void DestructSetup::startExtract()
{
    if (!QDir().mkpath(m_tempDir)) {
        fail(tr("Could not create %1.").arg(m_tempDir));
        return;
    }
    setStatus(tr("Extracting the source..."));
    appendLog(QStringLiteral("$ tar xzf %1 -C %2 --strip-components=1")
                  .arg(quoted(m_archivePath), quoted(m_tempDir)));
    startProcess(Step::Extract, QStringLiteral("tar"),
                 {QStringLiteral("xzf"), m_archivePath, QStringLiteral("-C"), m_tempDir,
                  QStringLiteral("--strip-components=1")});
}

void DestructSetup::startBuild()
{
    setStatus(tr("Building destruct. This can take a few minutes..."));
    appendLog(QStringLiteral("$ make build"));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!m_compiler.isEmpty())
        environment.insert(QStringLiteral("CC"), m_compiler);
    startProcess(Step::Build, QStringLiteral("make"), {QStringLiteral("build")},
                 m_tempDir, environment);
}

void DestructSetup::startVerify()
{
    const QString built = QDir(m_tempDir).filePath(QStringLiteral("destruct"));
    if (!QFileInfo(built).isExecutable()) {
        fail(tr("The build finished but %1 is missing.").arg(built));
        return;
    }
    setStatus(tr("Checking the built binary..."));
    appendLog(QStringLiteral("$ %1 version").arg(quoted(built)));
    startProcess(Step::Verify, built, {QStringLiteral("version")}, m_tempDir);
}

void DestructSetup::startProcess(Step step, const QString &program,
                                 const QStringList &arguments,
                                 const QString &workingDirectory,
                                 const QProcessEnvironment &environment)
{
    m_step = step;
    m_program = program;
    m_pendingLine.clear();
    m_killTimer.stop();

    m_process.setWorkingDirectory(workingDirectory);
    m_process.setProcessEnvironment(environment.isEmpty()
                                        ? QProcessEnvironment::systemEnvironment()
                                        : environment);
    m_process.setProgram(program);
    m_process.setArguments(arguments);
    m_process.start();
}

void DestructSetup::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_running)
        return;

    readOutput();
    flushOutput();

    if (m_cancelling) {
        finishCancellation();
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        QString message = tr("%1 failed (exit %2).")
                              .arg(stepName(m_step))
                              .arg(exitCode);
        if (!m_lastLine.isEmpty())
            message += QLatin1Char(' ') + m_lastLine;
        fail(message);
        return;
    }

    switch (m_step) {
    case Step::Download:
        startExtract();
        return;
    case Step::Extract:
        startBuild();
        return;
    case Step::Build:
        startVerify();
        return;
    case Step::Verify:
        if (!m_lastLine.contains(QStringLiteral("DeStruct"))) {
            fail(tr("The built binary did not answer like destruct."));
            return;
        }
        succeed();
        return;
    case Step::Idle:
        return;
    }
}

void DestructSetup::handleError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart || !m_running)
        return;
    fail(tr("Could not start %1: %2").arg(m_program, m_process.errorString()));
}

void DestructSetup::readOutput()
{
    m_pendingLine += QString::fromUtf8(m_process.readAll());
    if (m_pendingLine.size() > setupLogLimit)
        m_pendingLine = m_pendingLine.right(64 * 1024);

    int newline;
    while ((newline = m_pendingLine.indexOf(QLatin1Char('\n'))) >= 0) {
        appendLog(m_pendingLine.left(newline));
        m_pendingLine.remove(0, newline + 1);
    }
}

void DestructSetup::flushOutput()
{
    if (m_pendingLine.isEmpty())
        return;
    appendLog(m_pendingLine);
    m_pendingLine.clear();
}

void DestructSetup::appendLog(const QString &line)
{
    QString clean = line;
    clean.remove(QLatin1Char('\r'));
    m_log->append(clean);
    const QString trimmed = clean.trimmed();
    if (!trimmed.isEmpty())
        m_lastLine = trimmed;
}

void DestructSetup::setStatus(const QString &status)
{
    if (status == m_status)
        return;
    m_status = status;
    emit statusChanged();
}

void DestructSetup::fail(const QString &message)
{
    m_running = false;
    m_cancelling = false;
    m_step = Step::Idle;
    m_error = message;
    setStatus(tr("Setup failed."));
    appendLog(QStringLiteral("Error: ") + message);
    cleanupArtifacts();
    emit stateChanged();
}

void DestructSetup::succeed()
{
    QDir dataDir(m_dataDir);
    const QString backup = dataDir.filePath(QStringLiteral("DeStruct-old"));
    QDir(backup).removeRecursively();

    const bool hadOld = QFileInfo::exists(m_sourceDir);
    if (hadOld && !dataDir.rename(m_sourceDir, backup)) {
        fail(tr("Could not replace %1.").arg(m_sourceDir));
        return;
    }
    if (!dataDir.rename(m_tempDir, m_sourceDir)) {
        if (hadOld)
            dataDir.rename(backup, m_sourceDir);
        fail(tr("Could not move the build into %1.").arg(m_sourceDir));
        return;
    }
    QDir(backup).removeRecursively();
    QFile::remove(m_archivePath);

    if (!QFileInfo(binaryPath()).isExecutable()) {
        fail(tr("The built binary is not executable: %1").arg(binaryPath()));
        return;
    }

    m_running = false;
    m_cancelling = false;
    m_succeeded = true;
    m_step = Step::Idle;
    setStatus(tr("DeStruct is ready."));
    appendLog(tr("Installed destruct at %1").arg(binaryPath()));
    emit stateChanged();
    emit installed(binaryPath());
}

void DestructSetup::finishCancellation()
{
    m_running = false;
    m_cancelling = false;
    m_step = Step::Idle;
    // Cancelling is a choice, not a failure: return to the ask view rather
    // than painting the dialog red.
    m_error.clear();
    setStatus(tr("Setup cancelled."));
    appendLog(tr("Setup cancelled."));
    cleanupArtifacts();
    emit stateChanged();
}

void DestructSetup::cleanupArtifacts()
{
    removeTemp();
    QFile::remove(m_archivePath);
}

void DestructSetup::removeTemp()
{
    if (m_tempDir.isEmpty())
        return;
    QDir(m_tempDir).removeRecursively();
}

void DestructSetup::cancel()
{
    if (!m_running || m_cancelling)
        return;
    m_cancelling = true;
    setStatus(tr("Cancelling setup..."));
    appendLog(tr("Cancelling setup..."));
    emit stateChanged();
    m_process.terminate();
    m_killTimer.start();
}

QString DestructSetup::stepName(Step step) const
{
    switch (step) {
    case Step::Download:
        return tr("The download");
    case Step::Extract:
        return tr("The extraction");
    case Step::Build:
        return tr("The build");
    case Step::Verify:
        return tr("The check");
    case Step::Idle:
        break;
    }
    return tr("Setup");
}
