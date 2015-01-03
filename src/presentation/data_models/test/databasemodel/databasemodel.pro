#-------------------------------------------------
#
# Project created by QtCreator 2015-01-03T18:53:42
#
#-------------------------------------------------

QT       += testlib

TOP_DIR = ../../../../..

QMAKE_CXXFLAGS += -std=c++11
DEFINES += GUTIL_CORE_QT_ADAPTERS

TARGET = tst_databasemodeltest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

INCLUDEPATH += $$TOP_DIR/include $$TOP_DIR/gutil/include
LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lgrypto_ui \
    -lgrypto_core


SOURCES += tst_databasemodeltest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"
