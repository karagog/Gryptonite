QT += widgets sql xml

win32{
QT += winextras
RC_FILE = gryptonite.rc
}

TEMPLATE = app

TOP_DIR = ../../..

DESTDIR = $$TOP_DIR/bin
TARGET = gryptonite
QMAKE_CXXFLAGS += -std=c++11

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
    application.h \
    about.h \
    preferences_edit.h \
    settings.h \
    entry_popup.h \
    legacymanager.h

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    application.cpp \
    about.cpp \
    preferences_edit.cpp \
    legacymanager.cpp

FORMS += \
    mainwindow.ui \
    preferences_edit.ui \
    entry_popup.ui
