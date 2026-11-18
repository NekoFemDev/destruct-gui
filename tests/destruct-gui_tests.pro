QT += core gui qml quick quickcontrols2 testlib dbus concurrent

CONFIG += c++17 testcase
TARGET = destruct-gui_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/filelistmodel.h \
    ../src/filepicker.h \
    ../src/formats.h \
    ../src/logmodel.h \
    ../src/portalfilepicker.h \
    ../src/searchmodel.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    destruct-gui_tests.cpp \
    ../src/backend.cpp \
    ../src/filelistmodel.cpp \
    ../src/formats.cpp \
    ../src/logmodel.cpp \
    ../src/portalfilepicker.cpp \
    ../src/searchmodel.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp
