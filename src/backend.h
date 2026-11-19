#pragma once

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <atomic>
#include <memory>

#include "formats.h"

class DestructSetup;
class FileListModel;
class FilePicker;
class LogModel;
class SearchModel;

// Backend owns one job at a time: the chosen input, the matching destruct
// command, the options that command understands, the child process, and the
// files it produced. QML gets state and translated labels; everything that
// touches the disk or a process lives here.
class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString inputPath READ inputPath NOTIFY inputChanged)
    Q_PROPERTY(QString inputName READ inputName NOTIFY inputChanged)
    Q_PROPERTY(QString inputDirectory READ inputDirectory NOTIFY inputChanged)
    Q_PROPERTY(qint64 inputSize READ inputSize NOTIFY inputChanged)
    Q_PROPERTY(QString formatKind READ formatKind NOTIFY inputChanged)
    Q_PROPERTY(QString architecture READ architecture NOTIFY inputChanged)
    Q_PROPERTY(bool aarch64 READ aarch64 NOTIFY inputChanged)

    Q_PROPERTY(QStringList availableCommands READ availableCommands NOTIFY commandsChanged)
    Q_PROPERTY(QString command READ command WRITE setCommand NOTIFY commandChanged)
    Q_PROPERTY(bool outputUsed READ outputUsed NOTIFY commandChanged)

    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputChanged)
    Q_PROPERTY(QString metadataPath READ metadataPath NOTIFY metadataChanged)
    Q_PROPERTY(QString elfMetadataPath READ elfMetadataPath NOTIFY elfMetadataChanged)
    Q_PROPERTY(QString offsetsPath READ offsetsPath NOTIFY offsetsChanged)

    Q_PROPERTY(bool decompile READ decompile WRITE setDecompile NOTIFY optionsChanged)
    Q_PROPERTY(bool splitFunctions READ splitFunctions WRITE setSplitFunctions NOTIFY optionsChanged)
    Q_PROPERTY(bool crossReferences READ crossReferences WRITE setCrossReferences NOTIFY optionsChanged)
    Q_PROPERTY(bool simplifyCfg READ simplifyCfg WRITE setSimplifyCfg NOTIFY optionsChanged)
    Q_PROPERTY(bool emitLLVM READ emitLLVM WRITE setEmitLLVM NOTIFY optionsChanged)
    Q_PROPERTY(bool regsAsVars READ regsAsVars WRITE setRegsAsVars NOTIFY optionsChanged)
    Q_PROPERTY(bool noLocationComments READ noLocationComments WRITE setNoLocationComments NOTIFY optionsChanged)
    Q_PROPERTY(bool verbose READ verbose WRITE setVerbose NOTIFY optionsChanged)

    Q_PROPERTY(bool canRun READ canRun NOTIFY validationChanged)
    Q_PROPERTY(QString validationCode READ validationCode NOTIFY validationChanged)
    Q_PROPERTY(QString validationDetail READ validationDetail NOTIFY validationChanged)

    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool cancelling READ cancelling NOTIFY stateChanged)
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY elapsedChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY noticeChanged)

    Q_PROPERTY(QObject *files READ files CONSTANT)
    Q_PROPERTY(QObject *log READ log CONSTANT)
    Q_PROPERTY(QObject *searchResults READ searchResults CONSTANT)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY resultsChanged)
    Q_PROPERTY(qint64 resultBytes READ resultBytes NOTIFY resultsChanged)
    Q_PROPERTY(QString primaryRelativePath READ primaryRelativePath NOTIFY resultsChanged)
    Q_PROPERTY(bool collectionResult READ collectionResult NOTIFY resultsChanged)

    Q_PROPERTY(QString previewRelativePath READ previewRelativePath NOTIFY previewChanged)
    Q_PROPERTY(QString previewName READ previewName NOTIFY previewChanged)
    Q_PROPERTY(QString previewFolder READ previewFolder NOTIFY previewChanged)
    Q_PROPERTY(qint64 previewSize READ previewSize NOTIFY previewChanged)
    Q_PROPERTY(QString previewText READ previewText NOTIFY previewChanged)
    Q_PROPERTY(bool previewTextual READ previewTextual NOTIFY previewChanged)
    Q_PROPERTY(bool previewTruncated READ previewTruncated NOTIFY previewChanged)
    Q_PROPERTY(int previewLine READ previewLine NOTIFY previewChanged)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY previewChanged)

    Q_PROPERTY(QString binaryPath READ binaryPath NOTIFY binaryChanged)
    Q_PROPERTY(QString binaryVersion READ binaryVersion NOTIFY binaryChanged)
    Q_PROPERTY(bool binaryReady READ binaryReady NOTIFY binaryChanged)
    Q_PROPERTY(bool binaryProbing READ binaryProbing NOTIFY binaryChanged)
    Q_PROPERTY(QObject *setup READ setup CONSTANT)

    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)

    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(int searchCount READ searchCount NOTIFY searchCountChanged)

public:
    enum State {
        Idle = 0,
        Running = 1,
        Succeeded = 2,
        Failed = 3,
        Cancelled = 4,
    };
    Q_ENUM(State)

    explicit Backend(FilePicker *filePicker = nullptr, QObject *parent = nullptr);
    ~Backend() override;

    QString inputPath() const { return m_inputPath; }
    QString inputName() const;
    QString inputDirectory() const;
    qint64 inputSize() const { return m_inputSize; }
    QString formatKind() const;
    QString architecture() const { return m_info.architecture; }
    bool aarch64() const { return m_info.aarch64; }

    QStringList availableCommands() const { return m_availableCommands; }
    QString command() const { return m_command; }
    bool outputUsed() const { return m_command != QStringLiteral("elf-info"); }

    QString outputPath() const { return m_outputPath; }
    QString metadataPath() const { return m_metadataPath; }
    QString elfMetadataPath() const { return m_elfMetadataPath; }
    QString offsetsPath() const { return m_offsetsPath; }

    bool decompile() const { return m_decompile; }
    bool splitFunctions() const { return m_splitFunctions; }
    bool crossReferences() const { return m_crossReferences; }
    bool simplifyCfg() const { return m_simplifyCfg; }
    bool emitLLVM() const { return m_emitLLVM; }
    bool regsAsVars() const { return m_regsAsVars; }
    bool noLocationComments() const { return m_noLocationComments; }
    bool verbose() const { return m_verbose; }

    bool canRun() const { return !running() && m_validationCode.isEmpty(); }
    QString validationCode() const { return m_validationCode; }
    QString validationDetail() const { return m_validationDetail; }

    bool running() const { return m_state == Running; }
    bool cancelling() const { return m_cancelling; }
    int state() const { return m_state; }
    qint64 elapsedMs() const;
    QString activity() const { return m_activity; }
    QString errorText() const { return m_errorText; }
    QString noticeText() const { return m_noticeText; }

    QObject *files() const;
    QObject *log() const;
    QObject *searchResults() const;
    int fileCount() const;
    qint64 resultBytes() const;
    QString primaryRelativePath() const { return m_primaryRelativePath; }
    bool collectionResult() const { return m_collectionResult; }

    QString previewRelativePath() const { return m_previewRelativePath; }
    QString previewName() const;
    QString previewFolder() const;
    qint64 previewSize() const { return m_previewSize; }
    QString previewText() const { return m_previewText; }
    bool previewTextual() const { return m_previewTextual; }
    bool previewTruncated() const { return m_previewTruncated; }
    int previewLine() const { return m_previewLine; }
    bool hasPreview() const { return !m_previewRelativePath.isEmpty(); }

    QString binaryPath() const { return m_binaryPath; }
    QString binaryVersion() const { return m_binaryVersion; }
    bool binaryReady() const { return m_binaryReady; }
    bool binaryProbing() const { return m_binaryProbing; }
    QObject *setup() const;

    QStringList recentFiles() const { return m_recentFiles; }

    bool searching() const { return m_searching; }
    QString searchQuery() const { return m_searchQuery; }
    int searchCount() const;

    // The exact argv and its shell rendering, exposed for the tests and for
    // the log's command line.
    QStringList arguments() const;
    QString commandLine() const;

    Q_INVOKABLE void setInputPath(const QString &path);
    Q_INVOKABLE void acceptDrop(const QUrl &url);
    Q_INVOKABLE void setMetadataPath(const QString &path);
    Q_INVOKABLE void setElfMetadataPath(const QString &path);
    Q_INVOKABLE void setOutputPath(const QString &path);
    Q_INVOKABLE void setOffsetsPath(const QString &path);
    Q_INVOKABLE void setBinaryPath(const QString &path);

    void setCommand(const QString &command);
    void setDecompile(bool value);
    void setSplitFunctions(bool value);
    void setCrossReferences(bool value);
    void setSimplifyCfg(bool value);
    void setEmitLLVM(bool value);
    void setRegsAsVars(bool value);
    void setNoLocationComments(bool value);
    void setVerbose(bool value);

    Q_INVOKABLE void chooseInput();
    Q_INVOKABLE void chooseMetadata();
    Q_INVOKABLE void chooseElfMetadata();
    Q_INVOKABLE void chooseOutput();
    Q_INVOKABLE void chooseOffsets();
    Q_INVOKABLE void chooseBinary();

    Q_INVOKABLE void run();
    Q_INVOKABLE void cancel();

    Q_INVOKABLE void openPreview(const QString &relativePath, int line);
    Q_INVOKABLE void openPreviewExternally();
    Q_INVOKABLE void openOutputFolder();
    Q_INVOKABLE void copyLog();
    Q_INVOKABLE void copyCommand();
    Q_INVOKABLE void filterFiles(const QString &filter);

    Q_INVOKABLE void searchOutput(const QString &query, bool caseSensitive);
    Q_INVOKABLE void clearSearch();

    Q_INVOKABLE void useRecent(const QString &path);
    Q_INVOKABLE void clearRecent();
    Q_INVOKABLE void dismissNotice();

signals:
    void inputChanged();
    void commandsChanged();
    void commandChanged();
    void outputChanged();
    void metadataChanged();
    void elfMetadataChanged();
    void offsetsChanged();
    void optionsChanged();
    void validationChanged();
    void stateChanged();
    void elapsedChanged();
    void activityChanged();
    void noticeChanged();
    void resultsChanged();
    void previewChanged();
    void binaryChanged();
    void recentFilesChanged();
    void searchingChanged();
    void searchQueryChanged();
    void searchCountChanged();

private:
    void connectPicker();
    void handlePickedPath(int mode, const QString &path);
    void discoverBinary();
    void probeBinary();
    void updateAvailableCommands();
    void pickCommandForInput();
    void resetOptions();
    void applyDefaultOutput();
    void refreshValidation();
    void setValidation(const QString &code, const QString &detail = {});
    void resetResults();
    void scanResults();
    QString findPrimaryArtifact() const;
    QString resolveInOutput(const QString &relativePath) const;
    void appendLog(const QString &line);
    void flushPendingLine();
    void finishJob(int exitCode, QProcess::ExitStatus status);
    void failToStart(const QString &message);
    void addRecent(const QString &path);
    void saveRecent();
    void setState(State state);
    void showNotice(const QString &text);
    static QStringList commandsFor(const formats::Info &info);
    bool outputWritable() const;
    static bool pathWritable(const QString &path);
    static QString trimmedError(const QString &line);

    FilePicker *m_picker = nullptr;
    DestructSetup *m_setup = nullptr;
    FileListModel *m_files = nullptr;
    LogModel *m_logModel = nullptr;
    SearchModel *m_searchModel = nullptr;

    QString m_inputPath;
    qint64 m_inputSize = 0;
    formats::Info m_info;

    QStringList m_availableCommands;
    QString m_command;

    QString m_outputPath;
    QString m_metadataPath;
    QString m_elfMetadataPath;
    QString m_offsetsPath;

    bool m_decompile = false;
    bool m_splitFunctions = false;
    bool m_crossReferences = false;
    bool m_simplifyCfg = false;
    bool m_emitLLVM = false;
    bool m_regsAsVars = false;
    bool m_noLocationComments = false;
    bool m_verbose = false;

    QString m_validationCode;
    QString m_validationDetail;

    State m_state = Idle;
    bool m_cancelling = false;
    QProcess m_process;
    QTimer m_killTimer;
    QElapsedTimer m_elapsed;
    qint64 m_finalElapsedMs = 0;
    QTimer m_tick;
    QString m_pendingLine;
    QString m_lastErrorLine;
    QString m_activity;
    QString m_errorText;
    QString m_noticeText;
    QTimer m_noticeTimer;

    QString m_primaryRelativePath;
    bool m_collectionResult = false;

    QString m_previewRelativePath;
    QString m_previewText;
    qint64 m_previewSize = 0;
    bool m_previewTextual = true;
    bool m_previewTruncated = false;
    int m_previewLine = 0;

    QString m_binaryPath;
    QString m_binaryVersion;
    bool m_binaryReady = false;
    bool m_binaryProbing = false;
    QProcess m_probe;
    QTimer m_probeTimeout;

    QStringList m_recentFiles;

    bool m_searching = false;
    QString m_searchQuery;
    int m_searchGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_searchCancel;
};
