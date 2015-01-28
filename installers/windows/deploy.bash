#! /bin/bash

TOP_DIR=../..

cd $TOP_DIR

cp lib/*.dll bin

cd bin

# This copies all the Qt dependencies
windeployqt gryptonite.exe
windeployqt grypto_core.dll
windeployqt grypto_ui.dll
