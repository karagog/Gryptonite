#-------------------------------------------------
#
# Project created by QtCreator 2014-08-09T11:30:01
#
#-------------------------------------------------

QT       += testlib

QT       -= gui

TOP_DIR = ../../../../..

TARGET = tst_cryptortest
CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH += $$TOP_DIR/include
LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lGryptoCore \
    -lGUtil \
    -lcryptopp

TEMPLATE = app


SOURCES += tst_cryptortest.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"
