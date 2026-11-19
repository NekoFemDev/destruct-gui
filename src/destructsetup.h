#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

class LogModel;

// DestructSetup downloads DeStruct's source and builds the `destruct` CLI on
// request. It only ever writes inside the app's own data directory, and it
// never runs unless the user clicks "Download and build" in the setup dialog.
class DestructSetup : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool cancelling READ cancelling NOTIFY stateChanged)
    Q_PROPERTY(bool succeeded READ succeeded NOTIFY stateChanged)
    Q_PROPERTY(bool buildSupported READ buildSupported CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString sourceDirectory READ sourceDirectory CONSTANT)
    Q_PROPERTY(QString binaryPath READ binaryPath CONSTANT)
    Q_PROPERTY(QObject *log READ log CONSTANT)

public:
    // dataDirectory is empty in the app; the tests pass a temporary one.
    explicit DestructSetup(const QString &dataDirectory = {}, QObject *parent = nullptr);
    ~DestructSetup() override;

    bool running() const { return m_running; }
    bool cancelling() const { return m_cancelling; }
    bool succeeded() const { return m_succeeded; }
    bool buildSupported() const;
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    QString sourceDirectory() const { return m_sourceDir; }
    QString binaryPath() const;
    QObject *log() const;

    Q_INVOKABLE void install();
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void statusChanged();
    void installed(const QString &path);

private:
    enum class Step {
        Idle,
        Download,
        Extract,
        Build,
        Verify,
    };

    void startDownload();
    void startExtract();
    void startBuild();
    void startVerify();
    void startProcess(Step step, const QString &program, const QStringList &arguments,
                      const QString &workingDirectory = {},
                      const QProcessEnvironment &environment = {});
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleError(QProcess::ProcessError error);
    void readOutput();
    void flushOutput();
    void appendLog(const QString &line);
    void setStatus(const QString &status);
    void fail(const QString &message);
    void succeed();
    void finishCancellation();
    void cleanupArtifacts();
    void removeTemp();
    QStringList missingTools();
    QString missingToolsMessage(const QStringList &missing) const;
    QString stepName(Step step) const;
    static QString downloadUrl();

    QString m_dataDir;
    QString m_sourceDir;
    QString m_tempDir;
    QString m_archivePath;
    QString m_status;
    QString m_error;
    QString m_pendingLine;
    QString m_lastLine;
    QString m_program;
    QString m_compiler;
    bool m_running = false;
    bool m_succeeded = false;
    bool m_cancelling = false;
    Step m_step = Step::Idle;
    LogModel *m_log = nullptr;
    QProcess m_process;
    QTimer m_killTimer;
};
