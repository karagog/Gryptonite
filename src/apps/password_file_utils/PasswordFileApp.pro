
QT       += core

QT       -= gui

DESTDIR = ../bin
TARGET = GryptoFileUtility
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += .. ../GUtil
LIBS += -L../lib \
    -lGUtilCore \
    -lGUtilInterfaces \
    -lGUtilCustom \
    -lGUtilDataObjects \
    -lGUtilBusinessObjects \
    -lGryptoCommon \
    -lcryptopp.dll

SOURCES += \
    main.cpp \
    passwordfileconverter.cpp

include(Legacy/Legacy.pri)

HEADERS += \
    passwordfileconverter.h
