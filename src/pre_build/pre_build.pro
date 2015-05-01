TEMPLATE = lib
CONFIG += staticlib

TOP_DIR = ../..

HEADER_CMD = python $$TOP_DIR/gutil/scripts/GenerateHeaders.py
HEADER_PREFIX =

# Directory patterns for which we want to ignore all headers
IGNORE_PATHS = Test

# File patterns to ignore
IGNORE_FILES = *_p.h,ui_*.h

HEADERGEN_TARGET_DIRS = core,presentation


headers.commands = $$HEADER_CMD \
                        --working-dir=.. \
                        --output-dir=../include/grypto \
                        --input-dirs=$$HEADERGEN_TARGET_DIRS \
                        --ignore-path=$$IGNORE_PATHS \
                        --output-prefix=$$HEADER_PREFIX \
                        --ignore-patterns=$$IGNORE_FILES

PRE_TARGETDEPS =  headers

QMAKE_EXTRA_TARGETS =  headers
