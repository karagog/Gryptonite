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
unix: QMAKE_RPATHDIR = /usr/local/lib/gryptonite

TEMPLATE = lib
CONFIG += plugin

QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$TOP_DIR/include $$TOP_DIR/gutil/include

LIBS += -L$$TOP_DIR/lib \
    -lgrypto_legacy

SOURCES += legacyplugin.cpp

HEADERS += legacyplugin.h
