
QT += sql xml

TOP_DIR = ../..

DESTDIR = $$TOP_DIR/lib
TARGET = grypto_core
TEMPLATE = lib
QMAKE_CXXFLAGS += -std=c++11

unix: VERSION = 3.0.0

CONFIG(debug, debug|release) {
    #message(Preparing debug build)
    DEFINES += DEBUG
}
else {
    #message(Preparing release build)
    DEFINES += QT_NO_DEBUG_OUTPUT
    unix: QMAKE_RPATHDIR =
}

DEFINES += GUTIL_CORE_QT_ADAPTERS


INCLUDEPATH += $$TOP_DIR/gutil/include $$TOP_DIR/include

win32{
LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lGUtilQt \
    -lGUtilCryptoPP \
    -lGUtil \
    -lcryptopp
}

SOURCES +=

HEADERS += \
    common.h

include(data_access/data_access.pri)
include(data_objects/data_objects.pri)
include(interfaces/interfaces.pri)
include(utils/utils.pri)
