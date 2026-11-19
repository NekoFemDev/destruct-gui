#pragma once

#include "filepicker.h"

// The native Windows dialogs. QFileDialog needs QApplication, which main()
// creates on Windows only; the rest of the app still talks to FilePicker.
class WindowsFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit WindowsFilePicker(QObject *parent = nullptr);

    void choose(Mode mode, const QUrl &suggested = {}) override;
};
