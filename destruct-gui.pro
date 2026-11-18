QT += core gui qml quick quickcontrols2 dbus concurrent

CONFIG += c++17 release
TARGET = destruct-gui
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/filelistmodel.h \
    src/filepicker.h \
    src/formats.h \
    src/logmodel.h \
    src/portalfilepicker.h \
    src/searchmodel.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/filelistmodel.cpp \
    src/formats.cpp \
    src/logmodel.cpp \
    src/portalfilepicker.cpp \
    src/searchmodel.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
