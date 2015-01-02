QT += widgets
QT += sql

TOP_DIR = ../..

TEMPLATE = lib
DESTDIR = $$TOP_DIR/lib

TARGET = grypto_ui

DEFINES += GUTIL_CORE_QT_ADAPTERS
QMAKE_CXXFLAGS += -std=c++0x

CONFIG(debug, debug|release) {
    #message(Preparing debug build)
    DEFINES += DEBUG
}
else {
    #message(Preparing release build)
}


INCLUDEPATH += $$TOP_DIR/gutil/include $$TOP_DIR/include

win32{
LIBS += -L$$TOP_DIR/lib -L$$TOP_DIR/gutil/lib \
    -lgrypto_core \
    -lGUtilQt \
    -lGUtilCryptoPP \
    -lGUtil
}

RESOURCES += grypto_ui.qrc

include(controls/controls.pri)
include(custom/custom.pri)
include(data_models/data_models.pri)
include(forms/forms.pri)
include(ui/ui.pri)
include(utils/utils.pri)
