#pragma once

#include <QString>
#include <QStringList>

// Pure format sniffing and command-line construction. Keeping this free of
// QObject means the tests can exercise every decision without a window, a
// process, or the destruct binary being installed.
namespace formats {

enum class Kind {
    Unknown,
    JVM,
    Dex,
    Apk,
    Hermes,
    JavaScript,
    Elf,
    Pe,
    Metadata,
};

struct Info {
    Kind kind = Kind::Unknown;
    // Human-readable machine name from the file header: AArch64, ARM, x86,
    // x86-64, or empty when the format does not expose one.
    QString architecture;
    bool aarch64 = false;
    bool zip = false;
    // APK contents, read from the archive's central directory.
    bool hasClassesDex = false;
    bool hasLibapp = false;
    // A shared library whose name or container says it is Flutter's Dart AOT
    // blob, so the "flutter" command is worth offering.
    bool flutterLibrary = false;
    // False when the extension claims a container whose magic is missing, so
    // the window can say so before the CLI fails deep inside a parser.
    bool signatureValid = true;
};

// detect reads only headers and, for zip containers, the central directory.
// The extension decides between JVM, DEX, and APK where the bytes cannot.
Info detect(const QString &path);

QString kindId(Kind kind);

// Paths the preview pane can show as text rather than treating as binary.
bool isTextPath(const QString &path);

// The default output directory sits next to the input, named after it, so a
// job's results are easy to find again without the GUI's history.
QString defaultOutputPath(const QString &inputPath);

struct JobOptions {
    QString command;
    QString input;
    QString metadata;
    QString output;
    QString offsets;
    bool decompile = false;
    bool splitFunctions = false;
    bool crossReferences = false;
    bool simplifyCfg = false;
    bool emitLLVM = false;
    bool regsAsVars = false;
    bool noEnhance = false;
    bool noLocationComments = false;
    bool deobfuscate = false;
    bool verbose = false;
    bool project = true;
};

// arguments builds the argv passed to `destruct`, exactly one element per
// argument, so paths with spaces need no quoting.
QStringList arguments(const JobOptions &options);

// commandLine is the shell-style rendering of arguments(), for the log and
// for copying a reproducible command out of the GUI.
QString commandLine(const JobOptions &options);

} // namespace formats
