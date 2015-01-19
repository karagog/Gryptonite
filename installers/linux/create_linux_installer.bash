#! /bin/bash

REPO_DIR=gryptonite_install
TEMP_DIR=gryptonite_v$1

cp ../Main/gryptonite $REPO_DIR/gryptonite
cp -r $REPO_DIR $TEMP_DIR

if [ -d "$TEMP_DIR/.svn" ]; then
	rm "$TEMP_DIR/.svn" -rf
fi;

tar -czf gryptonite_v$1.tar.gz $TEMP_DIR

rm "$TEMP_DIR" -rf
