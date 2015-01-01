QT += widgets network sql xml

TEMPLATE = app

TOP_DIR = ../../..

DESTDIR = $$TOP_DIR/bin
TARGET = gryptonite
QMAKE_CXXFLAGS += -std=c++0x

DEFINES += GUTIL_CORE_QT_ADAPTERS

INCLUDEPATH += $$TOP_DIR/gutil/include $$TOP_DIR/include

LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lgrypto_legacy \
    -lgrypto_ui \
    -lgrypto_core \
    -lGUtilQt \
    -lGUtilCryptoPP \
    -lGUtil \
    #-lGUtilTest \
    -lcryptopp

HEADERS += \
    mainwindow.h \
    lockout.h \
    application.h \
    about.h

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    lockout.cpp \
    application.cpp \
    about.cpp

FORMS += \
    mainwindow.ui
