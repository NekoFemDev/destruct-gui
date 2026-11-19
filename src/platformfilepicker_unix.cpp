#include "platformfilepicker.h"

#include "portalfilepicker.h"

FilePicker *createPlatformFilePicker(QObject *parent)
{
    return new PortalFilePicker(parent);
}
