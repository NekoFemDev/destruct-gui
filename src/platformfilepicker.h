#pragma once

class FilePicker;
class QObject;

// Creates the native picker for the platform: the XDG portal chooser on
// Wayland/Linux, QFileDialog on Windows. Keeping this behind one call lets
// the rest of the app stay free of platform #ifdefs.
FilePicker *createPlatformFilePicker(QObject *parent);
