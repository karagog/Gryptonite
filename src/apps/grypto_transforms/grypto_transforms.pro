QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TOP_DIR = ../../..

TEMPLATE = app
TARGET = grypto_transforms
unix: QMAKE_RPATHDIR =
DESTDIR = $$TOP_DIR/bin

QMAKE_CXXFLAGS += -std=c++11

DEFINES += GUTIL_CORE_QT_ADAPTERS

INCLUDEPATH += $$TOP_DIR/gutil/include $$TOP_DIR/include
LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lgrypto_ui \
    -lgrypto_core \
    -lGUtilQt \
    -lGUtilCryptoPP \
    -lGUtil \
    -lcryptopp

SOURCES += main.cpp \
    mainwindow.cpp

FORMS += \
    mainwindow.ui

HEADERS += \
    mainwindow.h \
    about.h
