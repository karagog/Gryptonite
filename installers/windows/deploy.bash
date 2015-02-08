#! /bin/bash

TOP_DIR=../..
SSL_DIR="C:\Program Files (x86)\OpenSSL\openssl-1.0.2-i386-win32"

cd $TOP_DIR

cp lib/*.dll bin
cp gutil/lib/*.dll bin
cp "$SSL_DIR"/*.dll bin

cd bin

# This copies all the Qt dependencies
windeployqt gryptonite.exe
windeployqt grypto_core.dll
windeployqt grypto_ui.dll
windeployqt GUtilQt.dll
