.pragma library

function commandLabel(id, decompile) {
    switch (id) {
    case "jvm": return qsTr("JVM · Java source")
    case "dex": return qsTr("DEX · Java source")
    case "hermes": return qsTr("Hermes · assembly")
    case "flutter": return qsTr("Flutter · Dart assembly")
    case "elf": return decompile ? qsTr("ELF · C decompilation") : qsTr("ELF · disassembly")
    case "il2cpp": return qsTr("IL2CPP · dump.cs")
    case "pe": return qsTr("PE · disassembly")
    case "js": return qsTr("JavaScript · deobfuscate")
    }
    return qsTr("Choose an action")
}

function commandHint(id, decompile) {
    switch (id) {
    case "jvm": return qsTr("Recovers Java source from .class and .jar files, with control flow and generics.")
    case "dex": return qsTr("Recovers Java source from .dex and .apk files, including multidex archives.")
    case "hermes": return qsTr("Writes hermes-dec .hasm assembly. Edit it, then reassemble with destruct assemble.")
    case "flutter": return qsTr("Disassembles the Dart AOT snapshot to asm.txt, with asm.bin for binary patching.")
    case "elf": return decompile
        ? qsTr("Recovers functions, control flow, and struct access as readable pseudo-C.")
        : qsTr("Disassembles native code. Turn on decompilation for readable pseudo-C.")
    case "il2cpp": return qsTr("Dumps Unity types, fields, and methods from libil2cpp.so and global-metadata.dat.")
    case "pe": return qsTr("Disassembles Windows PE executables and DLLs.")
    case "js": return qsTr("Beautifies JavaScript and folds obvious obfuscation into a readable .deobf.js.")
    }
    return ""
}

function formatLabel(kind, architecture) {
    switch (kind) {
    case "jvm": return qsTr("JVM bytecode")
    case "dex": return qsTr("Android DEX")
    case "apk": return qsTr("Android APK")
    case "hermes": return qsTr("Hermes bytecode")
    case "js": return qsTr("JavaScript")
    case "elf": return architecture !== "" ? qsTr("ELF · %1").arg(architecture) : qsTr("ELF binary")
    case "pe": return architecture !== "" ? qsTr("PE · %1").arg(architecture) : qsTr("PE binary")
    case "metadata": return qsTr("IL2CPP metadata")
    case "unknown": return qsTr("Unrecognized file")
    }
    return ""
}

function shortTag(kind, name) {
    switch (kind) {
    case "jvm": return name.toLowerCase().endsWith(".jar") ? "JAR" : "JVM"
    case "dex": return "DEX"
    case "apk": return "APK"
    case "hermes": return "HBC"
    case "js": return "JS"
    case "elf": return "ELF"
    case "pe": return "PE"
    case "metadata": return "DAT"
    }
    return "FILE"
}

function runLabel(command, decompile) {
    switch (command) {
    case "jvm":
    case "dex":
        return qsTr("Decompile")
    case "elf":
        return decompile ? qsTr("Decompile to C") : qsTr("Disassemble")
    case "hermes":
    case "flutter":
    case "pe":
        return qsTr("Disassemble")
    case "il2cpp":
        return qsTr("Dump")
    case "js":
        return qsTr("Deobfuscate")
    }
    return qsTr("Run")
}

function stateLabel(state) {
    switch (state) {
    case 1: return qsTr("Working")
    case 2: return qsTr("Finished")
    case 3: return qsTr("Failed")
    case 4: return qsTr("Cancelled")
    }
    return qsTr("Ready")
}

function validationMessage(code, detail) {
    switch (code) {
    case "missingBinary": return qsTr("The destruct binary was not found. Locate it with the status button in the header, or build it with make build in the DeStruct checkout.")
    case "probingBinary": return qsTr("Checking the destruct binary...")
    case "noInput": return qsTr("Choose a file to decompile.")
    case "missingLibrary": return qsTr("Choose libil2cpp.so to pair with the metadata file.")
    case "inputMissing": return qsTr("The input file no longer exists.")
    case "badSignature": return qsTr("This file does not look like a valid %1 file.").arg(detail)
    case "noCommand": return qsTr("This file type is not supported yet.")
    case "missingMetadata": return qsTr("IL2CPP needs global-metadata.dat as a second input.")
    case "metadataMissing": return qsTr("global-metadata.dat is missing or not a valid metadata file.")
    case "notAarch64": return qsTr("C decompilation needs an AArch64 binary; this file is %1.").arg(detail)
    case "offsetsMissing": return qsTr("The IL2CPP offsets dump no longer exists.")
    case "outputMissing": return qsTr("Choose an output folder.")
    case "outputIsFile": return qsTr("The output path is a file, not a folder.")
    case "outputNotWritable": return qsTr("The output folder is not writable.")
    }
    return ""
}

function formatSize(bytes, language) {
    // Touch the language so every size re-renders when Omarchy changes language.
    var languageToken = language
    var locale = Qt.locale()
    if (bytes < 1024)
        return bytes.toLocaleString(locale, 'f', 0) + " B"
    if (bytes < 1048576)
        return (bytes / 1024).toLocaleString(locale, 'f', bytes < 10240 ? 1 : 0) + " KB"
    if (bytes < 1073741824)
        return (bytes / 1048576).toLocaleString(locale, 'f', bytes < 10485760 ? 1 : 0) + " MB"
    return (bytes / 1073741824).toLocaleString(locale, 'f', 1) + " GB"
}

function formatDuration(milliseconds, language) {
    var languageToken = language
    var locale = Qt.locale()
    var seconds = Math.max(0, milliseconds) / 1000
    if (seconds < 10)
        return seconds.toLocaleString(locale, 'f', 1) + " s"
    if (seconds < 60)
        return seconds.toLocaleString(locale, 'f', 0) + " s"
    var minutes = Math.floor(seconds / 60)
    var rest = Math.round(seconds % 60)
    return qsTr("%1 min %2 s").arg(minutes).arg(rest)
}

function fileCountLabel(count) {
    return qsTr("%n files", "", count)
}

function filteredCountLabel(visible, total, language) {
    var languageToken = language
    var locale = Qt.locale()
    return qsTr("%1 of %2 files")
        .arg(visible.toLocaleString(locale, 'f', 0))
        .arg(total.toLocaleString(locale, 'f', 0))
}

function matchCountLabel(count) {
    return qsTr("%n matches", "", count)
}

function baseName(path) {
    var cut = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'))
    return cut >= 0 ? path.substring(cut + 1) : path
}

function directoryName(path) {
    var cut = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'))
    return cut > 0 ? path.substring(0, cut) : ""
}

function stem(path) {
    var name = baseName(path)
    var dot = name.lastIndexOf('.')
    return dot > 0 ? name.substring(0, dot) : name
}
