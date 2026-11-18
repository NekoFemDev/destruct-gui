#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filelistmodel.h"
#include "filepicker.h"
#include "formats.h"
#include "theme.h"
#include "translator.h"

namespace {

class FakePicker : public FilePicker {
    Q_OBJECT

public:
    void choose(Mode mode, const QUrl &suggested) override
    {
        requests.append({static_cast<int>(mode), suggested});
    }

    void respond(Mode mode, const QUrl &url)
    {
        emit selected(static_cast<int>(mode), url);
    }

    QList<QPair<int, QUrl>> requests;
};

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(contents);
    file.close();
}

QByteArray elfAarch64Header()
{
    QByteArray data(64, '\0');
    data[0] = 0x7f;
    data[1] = 'E';
    data[2] = 'L';
    data[3] = 'F';
    data[4] = 2;   // 64-bit
    data[5] = 1;   // little endian
    data[6] = 1;   // version
    data[18] = char(0xB7);
    data[19] = 0;
    return data;
}

QByteArray peHeader(quint16 machine)
{
    QByteArray data(0x100, '\0');
    data[0] = 'M';
    data[1] = 'Z';
    data[0x3C] = 0x40;  // PE header offset
    data[0x40] = 'P';
    data[0x41] = 'E';
    data[0x42] = 0;
    data[0x43] = 0;
    data[0x44] = char(machine & 0xff);
    data[0x45] = char((machine >> 8) & 0xff);
    return data;
}

// A shell script that behaves like destruct for the two commands the GUI
// cares about in tests: version, and a successful ELF run that writes files.
QString fakeDestruct(const QTemporaryDir &dir, bool succeed = true, bool slow = false)
{
    const QString path = dir.filePath(succeed ? (slow ? "destruct-slow" : "destruct-ok")
                                              : "destruct-fail");
    const QByteArray script = succeed
        ? R"(#!/bin/sh
if [ "$1" = "version" ]; then
    echo "DeStruct v9.9.9"
    exit 0
fi
out=""
prev=""
for arg in "$@"; do
    if [ "$prev" = "-o" ]; then out="$arg"; fi
    prev="$arg"
done
[ -n "$out" ] || exit 2
[ -d "$out" ] || { echo "Error: output folder was not created" >&2; exit 3; }
echo "Parsed fake ELF"
printf '// fake output\nint sub_1000(void) {\n    return 42;\n}\n' > "$out/libtest.decompiled.c"
printf '{"ok": true}\n' > "$out/functions.json"
echo "Decompiled 1 functions to $out"
exit 0
)"
        : R"(#!/bin/sh
if [ "$1" = "version" ]; then
    echo "DeStruct v9.9.9"
    exit 0
fi
echo "Error: boom" >&2
exit 1
)";
    if (slow) {
        const QByteArray slowScript = R"(#!/bin/sh
if [ "$1" = "version" ]; then echo "DeStruct v9.9.9"; exit 0; fi
sleep 10
exit 0
)";
        writeFile(path, slowScript);
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }
    writeFile(path, script);
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

} // namespace

class DestructGuiTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("destruct-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("destruct-gui-tests"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
        QSettings().clear();
    }

    void cleanup()
    {
        QSettings().clear();
    }

    // ------------------------------------------------------------------
    // Format sniffing
    // ------------------------------------------------------------------
    void detectsFormatsByMagicAndExtension()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        writeFile(dir.filePath("A.class"), QByteArray("\xca\xfe\xba\xbe", 4));
        QCOMPARE(int(formats::detect(dir.filePath("A.class")).kind), int(formats::Kind::JVM));

        writeFile(dir.filePath("a.jar"), QByteArray("PK\x03\x04rest", 8));
        QCOMPARE(int(formats::detect(dir.filePath("a.jar")).kind), int(formats::Kind::JVM));
        QVERIFY(formats::detect(dir.filePath("a.jar")).signatureValid);

        writeFile(dir.filePath("classes.dex"), QByteArray("dex\n035\0xxxx", 12));
        QCOMPARE(int(formats::detect(dir.filePath("classes.dex")).kind), int(formats::Kind::Dex));

        writeFile(dir.filePath("bundle.hbc"), QByteArray("\x60\0\0\0", 4));
        QCOMPARE(int(formats::detect(dir.filePath("bundle.hbc")).kind), int(formats::Kind::Hermes));

        writeFile(dir.filePath("app.js"), QByteArray("var x = 1;\n", 11));
        QCOMPARE(int(formats::detect(dir.filePath("app.js")).kind), int(formats::Kind::JavaScript));

        writeFile(dir.filePath("lib.so"), elfAarch64Header());
        const formats::Info elf = formats::detect(dir.filePath("lib.so"));
        QCOMPARE(int(elf.kind), int(formats::Kind::Elf));
        QCOMPARE(elf.architecture, QStringLiteral("AArch64"));
        QVERIFY(elf.aarch64);

        writeFile(dir.filePath("broken.so"), QByteArray("not an elf", 10));
        QVERIFY(!formats::detect(dir.filePath("broken.so")).signatureValid);

        writeFile(dir.filePath("program.exe"), peHeader(0x8664));
        const formats::Info pe = formats::detect(dir.filePath("program.exe"));
        QCOMPARE(int(pe.kind), int(formats::Kind::Pe));
        QCOMPARE(pe.architecture, QStringLiteral("x86-64"));

        const QByteArray metadata = QByteArray::fromHex("af1bb1fa00000000");
        writeFile(dir.filePath("global-metadata.dat"), metadata);
        QCOMPARE(int(formats::detect(dir.filePath("global-metadata.dat")).kind),
                 int(formats::Kind::Metadata));

        writeFile(dir.filePath("mystery.bin"), QByteArray("????", 4));
        QCOMPARE(int(formats::detect(dir.filePath("mystery.bin")).kind), int(formats::Kind::Unknown));
    }

    void detectsApkContentsFromCentralDirectory()
    {
        QTemporaryDir dir;
        QByteArray apk = QByteArray("PK\x03\x04", 4);
        apk.append(QByteArray(64, '\0'));
        apk.append("classes.dex");
        apk.append(QByteArray(20, '\0'));
        apk.append("lib/arm64-v8a/libapp.so");
        apk.append(QByteArray(20, '\0'));
        apk.append("assets/flutter_assets/kernel_blob.bin");
        writeFile(dir.filePath("app.apk"), apk);

        const formats::Info info = formats::detect(dir.filePath("app.apk"));
        QCOMPARE(int(info.kind), int(formats::Kind::Apk));
        QVERIFY(info.hasClassesDex);
        QVERIFY(info.hasLibapp);
        QVERIFY(info.flutterLibrary);
    }

    void defaultOutputSitsNextToInput()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("app.jar"), QByteArray("PK\x03\x04", 4));
        QCOMPARE(formats::defaultOutputPath(dir.filePath("app.jar")),
                 QDir(dir.path()).filePath("app_destruct"));
    }

    void textSuffixesArePreviewable()
    {
        QVERIFY(formats::isTextPath(QStringLiteral("a/Main.java")));
        QVERIFY(formats::isTextPath(QStringLiteral("dump.cs")));
        QVERIFY(formats::isTextPath(QStringLiteral("lib.decompiled.c")));
        QVERIFY(formats::isTextPath(QStringLiteral("index.hasm")));
        QVERIFY(!formats::isTextPath(QStringLiteral("lib.so")));
        QVERIFY(!formats::isTextPath(QStringLiteral("asm.bin")));
    }

    // ------------------------------------------------------------------
    // Command lines
    // ------------------------------------------------------------------
    void buildsJvmArguments()
    {
        formats::JobOptions options;
        options.command = QStringLiteral("jvm");
        options.input = QStringLiteral("/tmp/a b.jar");
        options.output = QStringLiteral("/tmp/out");
        options.deobfuscate = true;
        options.verbose = true;

        const QStringList args = formats::arguments(options);
        QCOMPARE(args, QStringList({QStringLiteral("jvm"),
                                    QStringLiteral("/tmp/a b.jar"),
                                    QStringLiteral("-o"), QStringLiteral("/tmp/out"),
                                    QStringLiteral("--deobfuscate"),
                                    QStringLiteral("-v")}));
        QCOMPARE(formats::commandLine(options),
                 QStringLiteral("destruct jvm '/tmp/a b.jar' -o /tmp/out --deobfuscate -v"));
    }

    void buildsElfDecompilerArguments()
    {
        formats::JobOptions options;
        options.command = QStringLiteral("elf");
        options.input = QStringLiteral("libnative.so");
        options.output = QStringLiteral("out");
        options.decompile = true;
        options.splitFunctions = true;
        options.crossReferences = true;
        options.simplifyCfg = true;
        options.emitLLVM = true;
        options.regsAsVars = true;
        options.noEnhance = true;
        options.noLocationComments = true;
        options.offsets = QStringLiteral("game_dump");

        const QStringList args = formats::arguments(options);
        QCOMPARE(args, QStringList({
            QStringLiteral("elf"), QStringLiteral("libnative.so"),
            QStringLiteral("-o"), QStringLiteral("out"),
            QStringLiteral("--decompile"), QStringLiteral("--split-functions"),
            QStringLiteral("--cross-references"), QStringLiteral("--simplify-cfg"),
            QStringLiteral("--emit-llvm"), QStringLiteral("--regs-as-vars"),
            QStringLiteral("--no-enhance"), QStringLiteral("--no-location-comments"),
            QStringLiteral("--il2cpp-offsets"), QStringLiteral("game_dump"),
        }));
    }

    void buildsIl2cppArgumentsInLibraryOrder()
    {
        formats::JobOptions options;
        options.command = QStringLiteral("il2cpp");
        options.input = QStringLiteral("libil2cpp.so");
        options.metadata = QStringLiteral("global-metadata.dat");
        options.output = QStringLiteral("out");
        options.verbose = true;

        QCOMPARE(formats::arguments(options), QStringList({
            QStringLiteral("il2cpp"), QStringLiteral("libil2cpp.so"),
            QStringLiteral("global-metadata.dat"),
            QStringLiteral("-o"), QStringLiteral("out"), QStringLiteral("-v"),
        }));
    }

    void buildsJsOutputName()
    {
        formats::JobOptions options;
        options.command = QStringLiteral("js");
        options.input = QStringLiteral("/tmp/app.min.js");
        options.output = QStringLiteral("/tmp/out");
        QCOMPARE(formats::arguments(options), QStringList({
            QStringLiteral("js"), QStringLiteral("/tmp/app.min.js"),
            QStringLiteral("-o"), QStringLiteral("/tmp/out/app.min.deobf.js"),
        }));
    }

    // ------------------------------------------------------------------
    // Backend state
    // ------------------------------------------------------------------
    void settingInputChoosesTheMatchingCommand()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        writeFile(dir.filePath("app.jar"), QByteArray("PK\x03\x04rest", 8));

        backend.setInputPath(dir.filePath("app.jar"));
        QCOMPARE(backend.command(), QStringLiteral("jvm"));
        QCOMPARE(backend.availableCommands(), QStringList{QStringLiteral("jvm")});
        QCOMPARE(backend.formatKind(), QStringLiteral("jvm"));
        QCOMPARE(backend.outputPath(), QDir(dir.path()).filePath("app_destruct"));
    }

    void libappPrefersFlutterAndOffersElf()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        writeFile(dir.filePath("libapp.so"), elfAarch64Header());

        backend.setInputPath(dir.filePath("libapp.so"));
        QCOMPARE(backend.command(), QStringLiteral("flutter"));
        QVERIFY(backend.availableCommands().contains(QStringLiteral("elf")));
        QVERIFY(backend.availableCommands().contains(QStringLiteral("il2cpp")));
    }

    void metadataSwitchesToIl2cpp()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        writeFile(dir.filePath("libil2cpp.so"), elfAarch64Header());
        writeFile(dir.filePath("global-metadata.dat"),
                  QByteArray::fromHex("af1bb1fa01000000"));

        backend.setInputPath(dir.filePath("libil2cpp.so"));
        QCOMPARE(backend.command(), QStringLiteral("elf"));
        backend.setMetadataPath(dir.filePath("global-metadata.dat"));
        QCOMPARE(backend.command(), QStringLiteral("il2cpp"));
        QVERIFY(backend.arguments().contains(QStringLiteral("il2cpp")));
        QVERIFY(backend.arguments().contains(dir.filePath("libil2cpp.so")));
        QVERIFY(backend.arguments().contains(dir.filePath("global-metadata.dat")));
    }

    void metadataAloneAsksForTheLibrary()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        writeFile(dir.filePath("global-metadata.dat"),
                  QByteArray::fromHex("af1bb1fa01000000"));

        backend.setInputPath(dir.filePath("global-metadata.dat"));
        QCOMPARE(backend.inputPath(), QString());
        QVERIFY(!backend.metadataPath().isEmpty());
        QCOMPARE(backend.command(), QStringLiteral("il2cpp"));
        QCOMPARE(backend.validationCode(), QStringLiteral("missingLibrary"));
    }

    void decompileIsAarch64Only()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        QByteArray arm = elfAarch64Header();
        arm[18] = 0x28;  // ARM32
        arm[19] = 0;
        writeFile(dir.filePath("libarm.so"), arm);

        backend.setInputPath(dir.filePath("libarm.so"));
        QCOMPARE(backend.command(), QStringLiteral("elf"));
        QVERIFY(!backend.decompile());
        backend.setDecompile(true);
        QVERIFY(!backend.decompile());
    }

    void turningDecompileOffClearsDependentOptions()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        writeFile(dir.filePath("lib.so"), elfAarch64Header());

        backend.setInputPath(dir.filePath("lib.so"));
        QVERIFY(backend.decompile());
        backend.setSplitFunctions(true);
        backend.setEmitLLVM(true);
        QVERIFY(backend.splitFunctions());
        QVERIFY(backend.emitLLVM());

        backend.setDecompile(false);
        QVERIFY(!backend.splitFunctions());
        QVERIFY(!backend.emitLLVM());
        QVERIFY(!backend.arguments().contains(QStringLiteral("--split-functions")));
    }

    void rejectsUnwritableOutput()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        writeFile(dir.filePath("lib.so"), elfAarch64Header());
        writeFile(dir.filePath("plain-file"), QByteArray("x"));

        backend.setInputPath(dir.filePath("lib.so"));
        backend.setOutputPath(dir.filePath("plain-file") + "/out");
        QCOMPARE(backend.validationCode(), QStringLiteral("outputNotWritable"));
        QVERIFY(!backend.canRun());
    }

    void missingBinaryIsReported()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        writeFile(dir.filePath("lib.so"), elfAarch64Header());
        backend.setInputPath(dir.filePath("lib.so"));
        backend.setBinaryPath(dir.filePath("no-such-binary"));
        QCOMPARE(backend.validationCode(), QStringLiteral("missingBinary"));
    }

    void pickerSelectionReachesTheBackend()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        const QString path = dir.filePath("app.jar");
        writeFile(path, QByteArray("PK\x03\x04rest", 8));

        backend.chooseInput();
        QCOMPARE(picker.requests.size(), 1);
        QCOMPARE(picker.requests.first().first, static_cast<int>(FilePicker::Input));
        picker.respond(FilePicker::Input, QUrl::fromLocalFile(path));
        QCOMPARE(backend.inputPath(), path);
    }

    void droppingAFolderSetsTheOutput()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        backend.acceptDrop(QUrl::fromLocalFile(dir.path()));
        QCOMPARE(backend.outputPath(), QDir::cleanPath(dir.path()));
    }

    // ------------------------------------------------------------------
    // Running a fake destruct
    // ------------------------------------------------------------------
    void runsAJobAndBrowsesResults()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        const QString binary = fakeDestruct(dir);
        writeFile(dir.filePath("libtest.so"), elfAarch64Header());

        backend.setBinaryPath(binary);
        QTRY_VERIFY_WITH_TIMEOUT(backend.binaryReady(), 5000);
        QCOMPARE(backend.binaryVersion(), QStringLiteral("9.9.9"));

        backend.setInputPath(dir.filePath("libtest.so"));
        backend.setOutputPath(dir.filePath("out"));
        QVERIFY(backend.canRun());

        backend.run();
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), int(Backend::Succeeded), 10000);
        QCOMPARE(backend.fileCount(), 2);
        QVERIFY(backend.resultBytes() > 0);
        QCOMPARE(backend.primaryRelativePath(), QStringLiteral("libtest.decompiled.c"));
        QCOMPARE(backend.recentFiles().first(), QDir(dir.path()).filePath("libtest.so"));

        backend.openPreview(QStringLiteral("libtest.decompiled.c"), 0);
        QVERIFY(backend.hasPreview());
        QVERIFY(backend.previewText().contains(QStringLiteral("sub_1000")));

        backend.searchOutput(QStringLiteral("sub_1000"), false);
        QTRY_COMPARE_WITH_TIMEOUT(backend.searchCount(), 1, 5000);
        QVERIFY(!backend.searching());
        QCOMPARE(backend.searchQuery(), QStringLiteral("sub_1000"));
        backend.clearSearch();
        QCOMPARE(backend.searchCount(), 0);

        backend.filterFiles(QStringLiteral("functions"));
        QCOMPARE(backend.files()->property("count").toInt(), 1);
        backend.filterFiles(QString());
        QCOMPARE(backend.files()->property("count").toInt(), 2);
    }

    void failedJobsSurfaceTheError()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        const QString binary = fakeDestruct(dir, false);
        writeFile(dir.filePath("libtest.so"), elfAarch64Header());
        backend.setBinaryPath(binary);
        QTRY_VERIFY(backend.binaryReady());
        backend.setInputPath(dir.filePath("libtest.so"));
        backend.setOutputPath(dir.filePath("out"));

        backend.run();
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), int(Backend::Failed), 10000);
        QCOMPARE(backend.errorText(), QStringLiteral("boom"));
        QCOMPARE(backend.fileCount(), 0);
        QVERIFY(backend.canRun());
    }

    void staleInputIsReported()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        backend.setInputPath(dir.filePath("gone.jar"));
        QCOMPARE(backend.validationCode(), QStringLiteral("inputMissing"));
        QVERIFY(!backend.canRun());
    }

    void cancellingAJobStopsIt()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        const QString binary = fakeDestruct(dir, true, true);
        writeFile(dir.filePath("libtest.so"), elfAarch64Header());
        backend.setBinaryPath(binary);
        QTRY_VERIFY(backend.binaryReady());
        backend.setInputPath(dir.filePath("libtest.so"));
        backend.setOutputPath(dir.filePath("out"));

        backend.run();
        QTRY_VERIFY(backend.running());
        backend.cancel();
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), int(Backend::Cancelled), 10000);
        QVERIFY(backend.canRun());
    }

    void previewStaysInsideTheOutputFolder()
    {
        FakePicker picker;
        Backend backend(&picker);
        QTemporaryDir dir;
        const QString binary = fakeDestruct(dir);
        writeFile(dir.filePath("libtest.so"), elfAarch64Header());
        writeFile(dir.filePath("outside.txt"), QByteArray("secret"));
        backend.setBinaryPath(binary);
        QTRY_VERIFY(backend.binaryReady());
        backend.setInputPath(dir.filePath("libtest.so"));
        backend.setOutputPath(dir.filePath("out"));

        backend.run();
        QTRY_COMPARE_WITH_TIMEOUT(backend.state(), int(Backend::Succeeded), 10000);
        backend.openPreview(QStringLiteral("../outside.txt"), 0);
        QVERIFY(!backend.hasPreview());
    }

    // ------------------------------------------------------------------
    // Theme and language, shared with every Omarchy app
    // ------------------------------------------------------------------
    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json");
        const QString command = dir.filePath("hyprctl");
        writeFile(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        writeFile(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        writeFile(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Open").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        writeFile(dir.path() + "/language", "ru\n");
        writeFile(dir.path() + "/en.json", R"({"%n files": {"one": "%n file", "other": "%n files"}})");
        writeFile(dir.path() + "/ru.json", R"({"Open": "Открыть", "%n files": {"one": "%n файл", "few": "%n файла", "many": "%n файлов", "other": "%n файла"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("ru"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Открыть"));
        QCOMPARE(translator.translate("Main", "%n files", nullptr, 21), QString("%n файл"));
        QCOMPARE(translator.translate("Main", "%n files", nullptr, 22), QString("%n файла"));
        QCOMPARE(translator.translate("Main", "%n files", nullptr, 25), QString("%n файлов"));
    }

private:
    QTemporaryDir m_settings;
};

QTEST_MAIN(DestructGuiTests)
#include "destruct-gui_tests.moc"
