#-------------------------------------------------
#
# Project created by QtCreator 2015-01-24T07:47:02
#
#-------------------------------------------------

QT       += core gui widgets

TOP_DIR = ../../..

DESTDIR = $$TOP_DIR/bin

DEFINES += GUTIL_CORE_QT_ADAPTERS

TARGET = grypto_legacy_plugin

CONFIG(debug, debug|release) {
    #message(Preparing debug build)
    DEFINES += DEBUG
}
else {
    #message(Preparing release build)
    DEFINES += QT_NO_DEBUG_OUTPUT
    unix: QMAKE_RPATHDIR =
}

TEMPLATE = lib
CONFIG += plugin

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$TOP_DIR/include $$TOP_DIR/gutil/include

LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lgrypto_legacy \
    -lgrypto_core \
    -lgrypto_ui \
    -lGUtil \
    -lcryptopp

SOURCES += legacyplugin.cpp

HEADERS += legacyplugin.h
