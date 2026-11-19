QT += core gui qml quick quickcontrols2 concurrent
unix: QT += dbus
win32: QT += widgets

CONFIG += c++17 release
TARGET = destruct-gui
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/destructsetup.h \
    src/filelistmodel.h \
    src/filepicker.h \
    src/formats.h \
    src/logmodel.h \
    src/platformfilepicker.h \
    src/searchmodel.h \
    src/theme.h \
    src/translator.h

unix: HEADERS += src/portalfilepicker.h
win32: HEADERS += src/windowsfilepicker.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/destructsetup.cpp \
    src/filelistmodel.cpp \
    src/formats.cpp \
    src/logmodel.cpp \
    src/searchmodel.cpp \
    src/theme.cpp \
    src/translator.cpp

unix: SOURCES += src/portalfilepicker.cpp src/platformfilepicker_unix.cpp
win32: SOURCES += src/windowsfilepicker.cpp src/platformfilepicker_win.cpp

win32: RC_ICONS = src/destruct-gui.ico

RESOURCES += src/resources.qrc
