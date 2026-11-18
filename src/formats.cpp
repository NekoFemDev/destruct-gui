#include "formats.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>

namespace {

QByteArray readPrefix(const QString &path, qint64 count)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.read(count);
}

bool startsWith(const QByteArray &data, const char *magic, int length)
{
    return data.size() >= length && qstrncmp(data.constData(), magic, length) == 0;
}

// A zip file's entry names live uncompressed in the central directory at the
// end of the archive. Reading the tail is enough to tell an APK with
// classes.dex or a Flutter libapp.so apart from a plain archive, without
// unpacking anything.
bool archiveContains(const QString &path, const QByteArray &needle)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const qint64 size = file.size();
    const qint64 window = qMin<qint64>(size, 4 * 1024 * 1024);
    if (window <= 0 || !file.seek(size - window))
        return false;
    return file.read(window).contains(needle);
}

QString architectureFromElf(const QByteArray &data)
{
    if (!startsWith(data, "\x7f" "ELF", 4) || data.size() < 20)
        return {};
    const bool littleEndian = static_cast<quint8>(data.at(5)) == 1;
    const auto *bytes = reinterpret_cast<const uchar *>(data.constData());
    const quint16 machine = littleEndian
        ? static_cast<quint16>(bytes[18] | (bytes[19] << 8))
        : static_cast<quint16>((bytes[18] << 8) | bytes[19]);
    switch (machine) {
    case 0xB7: return QStringLiteral("AArch64");
    case 0x28: return QStringLiteral("ARM");
    case 0x03: return QStringLiteral("x86");
    case 0x3E: return QStringLiteral("x86-64");
    case 0xF3: return QStringLiteral("RISC-V");
    default: return {};
    }
}

QString architectureFromPe(const QByteArray &data)
{
    if (!startsWith(data, "MZ", 2) || data.size() < 0x40)
        return {};
    const auto *bytes = reinterpret_cast<const uchar *>(data.constData());
    const qint64 peOffset = bytes[0x3C] | (bytes[0x3D] << 8) | (bytes[0x3E] << 16)
        | (static_cast<qint64>(bytes[0x3F]) << 24);
    if (peOffset + 6 > data.size()
        || qstrncmp(data.constData() + peOffset, "PE\0\0", 4) != 0)
        return {};
    const quint16 machine = static_cast<quint16>(
        bytes[peOffset + 4] | (bytes[peOffset + 5] << 8));
    switch (machine) {
    case 0xAA64: return QStringLiteral("AArch64");
    case 0x8664: return QStringLiteral("x86-64");
    case 0x014C: return QStringLiteral("x86");
    case 0x01C4: return QStringLiteral("ARM");
    default: return {};
    }
}

QString quoted(const QString &value)
{
    if (value.isEmpty())
        return QStringLiteral("''");
    const bool simple = std::all_of(value.begin(), value.end(), [](QChar c) {
        return c.isLetterOrNumber() || QStringLiteral("/._-+=:@%").contains(c);
    });
    if (simple)
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

} // namespace

namespace formats {

Info detect(const QString &path)
{
    Info info;
    const QFileInfo file(path);
    if (!file.isFile())
        return info;

    const QString suffix = file.suffix().toLower();
    const QString name = file.fileName().toLower();
    const QByteArray header = readPrefix(path, 4096);

    const bool elfMagic = startsWith(header, "\x7f" "ELF", 4);
    const bool peMagic = startsWith(header, "MZ", 2);
    const bool zipMagic = startsWith(header, "PK\x03\x04", 4)
        || startsWith(header, "PK\x05\x06", 4)
        || startsWith(header, "PK\x07\x08", 4);
    const bool dexMagic = startsWith(header, "dex\n", 4);
    const bool metadataMagic = header.size() >= 4
        && static_cast<quint8>(header.at(0)) == 0xAF
        && static_cast<quint8>(header.at(1)) == 0x1B
        && static_cast<quint8>(header.at(2)) == 0xB1
        && static_cast<quint8>(header.at(3)) == 0xFA;

    info.zip = zipMagic;

    if (metadataMagic || name == QStringLiteral("global-metadata.dat")) {
        info.kind = Kind::Metadata;
        info.signatureValid = metadataMagic;
        return info;
    }
    if (elfMagic) {
        info.kind = Kind::Elf;
        info.architecture = architectureFromElf(header);
        info.aarch64 = info.architecture == QStringLiteral("AArch64");
        info.flutterLibrary = file.fileName().startsWith(QStringLiteral("libapp"), Qt::CaseInsensitive);
        return info;
    }
    if (peMagic) {
        info.kind = Kind::Pe;
        info.architecture = architectureFromPe(header);
        return info;
    }
    if (dexMagic) {
        info.kind = Kind::Dex;
        return info;
    }

    if (zipMagic) {
        if (suffix == QStringLiteral("apk")) {
            info.kind = Kind::Apk;
            info.hasClassesDex = archiveContains(path, "classes.dex")
                || archiveContains(path, "classes2.dex");
            info.hasLibapp = archiveContains(path, "libapp.so");
            info.flutterLibrary = info.hasLibapp
                && archiveContains(path, "flutter_assets");
            return info;
        }
        if (suffix == QStringLiteral("jar")) {
            info.kind = Kind::JVM;
            return info;
        }
    }

    if (suffix == QStringLiteral("jar") || suffix == QStringLiteral("class")) {
        info.kind = Kind::JVM;
        info.signatureValid = suffix == QStringLiteral("class") || zipMagic;
        return info;
    }
    if (suffix == QStringLiteral("dex")) {
        info.kind = Kind::Dex;
        info.signatureValid = dexMagic;
        return info;
    }
    if (suffix == QStringLiteral("apk")) {
        info.kind = Kind::Apk;
        info.signatureValid = zipMagic;
        return info;
    }
    if (suffix == QStringLiteral("hbc") || suffix == QStringLiteral("bundle")) {
        info.kind = Kind::Hermes;
        return info;
    }
    if (suffix == QStringLiteral("js") || suffix == QStringLiteral("mjs")
        || suffix == QStringLiteral("cjs")) {
        info.kind = Kind::JavaScript;
        return info;
    }
    if (suffix == QStringLiteral("so") || suffix == QStringLiteral("elf")) {
        info.kind = Kind::Elf;
        info.signatureValid = false;
        info.flutterLibrary = file.fileName().startsWith(QStringLiteral("libapp"), Qt::CaseInsensitive);
        return info;
    }
    if (suffix == QStringLiteral("exe") || suffix == QStringLiteral("dll")
        || suffix == QStringLiteral("sys") || suffix == QStringLiteral("scr")) {
        info.kind = Kind::Pe;
        info.signatureValid = peMagic;
        return info;
    }

    return info;
}

QString kindId(Kind kind)
{
    switch (kind) {
    case Kind::JVM: return QStringLiteral("jvm");
    case Kind::Dex: return QStringLiteral("dex");
    case Kind::Apk: return QStringLiteral("apk");
    case Kind::Hermes: return QStringLiteral("hermes");
    case Kind::JavaScript: return QStringLiteral("js");
    case Kind::Elf: return QStringLiteral("elf");
    case Kind::Pe: return QStringLiteral("pe");
    case Kind::Metadata: return QStringLiteral("metadata");
    case Kind::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

bool isTextPath(const QString &path)
{
    static const QStringList suffixes{
        QStringLiteral("c"), QStringLiteral("h"), QStringLiteral("cc"),
        QStringLiteral("cpp"), QStringLiteral("hpp"), QStringLiteral("cs"),
        QStringLiteral("java"), QStringLiteral("kt"), QStringLiteral("js"),
        QStringLiteral("mjs"), QStringLiteral("cjs"), QStringLiteral("ts"),
        QStringLiteral("dart"), QStringLiteral("json"), QStringLiteral("txt"),
        QStringLiteral("hasm"), QStringLiteral("ll"), QStringLiteral("xml"),
        QStringLiteral("yml"), QStringLiteral("yaml"), QStringLiteral("md"),
        QStringLiteral("toml"), QStringLiteral("ini"), QStringLiteral("log"),
        QStringLiteral("py"), QStringLiteral("sh"), QStringLiteral("html"),
        QStringLiteral("css"), QStringLiteral("s"), QStringLiteral("ld"),
        QStringLiteral("map"), QStringLiteral("csv"),
    };
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

QString defaultOutputPath(const QString &inputPath)
{
    const QFileInfo input(inputPath);
    if (!input.isFile())
        return QDir::homePath() + QStringLiteral("/destruct-output");
    return input.absoluteDir().filePath(input.completeBaseName() + QStringLiteral("_destruct"));
}

QStringList arguments(const JobOptions &options)
{
    QStringList args;
    args << options.command;
    if (options.command == QStringLiteral("il2cpp")) {
        args << options.input << options.metadata;
    } else if (!options.input.isEmpty()) {
        args << options.input;
    }

    if (!options.output.isEmpty()) {
        if (options.command == QStringLiteral("js")) {
            const QString name = QFileInfo(options.input).completeBaseName()
                + QStringLiteral(".deobf.js");
            args << QStringLiteral("-o") << QDir(options.output).filePath(name);
        } else {
            args << QStringLiteral("-o") << options.output;
        }
    }

    if (options.command == QStringLiteral("elf") && options.decompile) {
        // Every one of these flags only makes sense on the decompiler path;
        // passing one with --decompile off would silently flip it back on.
        args << QStringLiteral("--decompile");
        if (options.splitFunctions)
            args << QStringLiteral("--split-functions");
        if (options.crossReferences)
            args << QStringLiteral("--cross-references");
        if (options.simplifyCfg)
            args << QStringLiteral("--simplify-cfg");
        if (options.emitLLVM)
            args << QStringLiteral("--emit-llvm");
        if (options.regsAsVars)
            args << QStringLiteral("--regs-as-vars");
        if (options.noEnhance)
            args << QStringLiteral("--no-enhance");
        if (options.noLocationComments)
            args << QStringLiteral("--no-location-comments");
        if (!options.offsets.isEmpty())
            args << QStringLiteral("--il2cpp-offsets") << options.offsets;
    }

    if (options.deobfuscate
        && (options.command == QStringLiteral("jvm")
            || options.command == QStringLiteral("dex")
            || options.command == QStringLiteral("flutter")
            || options.command == QStringLiteral("pe"))) {
        args << QStringLiteral("--deobfuscate");
    }
    if (!options.project
        && (options.command == QStringLiteral("jvm")
            || options.command == QStringLiteral("dex"))) {
        args << QStringLiteral("--no-project");
    }
    if (options.verbose)
        args << QStringLiteral("-v");
    return args;
}

QString commandLine(const JobOptions &options)
{
    QStringList parts{QStringLiteral("destruct")};
    const QStringList args = arguments(options);
    for (const QString &arg : args)
        parts << quoted(arg);
    return parts.join(QLatin1Char(' '));
}

} // namespace formats
