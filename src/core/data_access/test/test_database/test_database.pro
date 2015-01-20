#-------------------------------------------------
#
# Project created by QtCreator 2014-08-09T19:29:09
#
#-------------------------------------------------

QT       += sql testlib

QT       -= gui

TOP_DIR = ../../../../..

QMAKE_CXXFLAGS += -std=c++0x
DEFINES += GUTIL_CORE_QT_ADAPTERS

TARGET = tst_databasetest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app
INCLUDEPATH += $$TOP_DIR/include $$TOP_DIR/gutil/include
LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lgrypto_core \
    -lGUtil \
    -lGUtilQt \
    -lGUtilCryptoPP \
    -lcryptopp

SOURCES += tst_databasetest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"
