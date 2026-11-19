#include "platformfilepicker.h"

#include "windowsfilepicker.h"

FilePicker *createPlatformFilePicker(QObject *parent)
{
    return new WindowsFilePicker(parent);
}
