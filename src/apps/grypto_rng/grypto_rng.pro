QT       += core gui concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TOP_DIR = ../../..

TARGET = grypto_rng
TEMPLATE = app
DESTDIR = $$TOP_DIR/bin

QMAKE_CXXFLAGS += -std=c++11

DEFINES += GUTIL_CORE_QT_ADAPTERS

INCLUDEPATH += $$TOP_DIR/gutil/include $$TOP_DIR/include
LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lgrypto_ui \
    -lgrypto_core \
    -lGUtilQt \
    -lGUtilCryptoPP \
    -lGUtil

unix{
LIBS += -lcryptopp
}


SOURCES += \
    main.cpp \
    mainwindow.cpp \
    coinflipper.cpp \
    coinmodel.cpp \
    rollmodel.cpp \
    diceroller.cpp \
    datagenerator.cpp

HEADERS += \
    mainwindow.h \
    coinflipper.h \
    coinmodel.h \
    rollmodel.h \
    diceroller.h \
    about.h \
    datagenerator.h

FORMS += \
    mainwindow.ui \
    coinflipper.ui \
    diceroller.ui \
    datagenerator.ui
