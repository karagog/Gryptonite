
QT += sql xml

TOP_DIR = ../..

DESTDIR = $$TOP_DIR/lib
TARGET = grypto_legacy
TEMPLATE = lib
#CONFIG += staticlib
QMAKE_CXXFLAGS += -std=c++11

CONFIG(debug, debug|release) {
    #message(Preparing debug build)
    DEFINES += DEBUG
}
else {
    #message(Preparing release build)
}

DEFINES += GUTIL_CORE_QT_ADAPTERS


INCLUDEPATH += $$TOP_DIR/include $$TOP_DIR/gutil/include

LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lGUtilCryptoPP \
    -lGUtil \
    -lcryptopp

SOURCES += \
    password_file.cpp \
    file_entry.cpp \
    attribute.cpp \
    encryption.cpp \
    legacyutils.cpp

HEADERS += \
    password_file.h \
    file_entry.h \
    attribute.h \
    default_attribute_info.h \
    encryption.h \
    legacyutils.h
