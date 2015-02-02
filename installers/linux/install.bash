#! /bin/bash

# You may update these locations to install wherever you want. BUT
#  BE CAREFUL if you update this because the uninstaller will
#  remove the entire directory!
INSTALL_BASE_DIR="/usr/local"
INSTALL_DIR_LIBS="$INSTALL_BASE_DIR/lib/gryptonite"
INSTALL_DIR_BIN="$INSTALL_BASE_DIR/bin"

# You will call this startup script to launch gryptonite. It exports
#  the proper LD_LIBRARY_PATH and calls the executable in the lib directory
START_LOC="$INSTALL_DIR_BIN/gryptonite"
START_LOC_TRANSFORMS="$INSTALL_DIR_BIN/grypto_transforms"

# This is a fancy expression that gets the install script's current directory
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $SCRIPT_DIR

# Create the installation directories
if [ ! -d "$INSTALL_DIR_LIBS" ]; then
    mkdir -p "$INSTALL_DIR_LIBS"
fi

if [ ! -d "$INSTALL_DIR_BIN" ]; then
    mkdir -p "$INSTALL_DIR_BIN"
fi

# Create an uninstall script
echo "#! /bin/bash"                 > uninstall.bash
echo "rm $START_LOC"                >> uninstall.bash
echo "rm $START_LOC_TRANSFORMS"     >> uninstall.bash
echo "rm -rf $INSTALL_DIR_LIBS"     >> uninstall.bash
chmod +x uninstall.bash

# Create a startup script
echo "#! /bin/bash"                                 > $START_LOC
echo "export LD_LIBRARY_PATH=$INSTALL_DIR_LIBS"      >> $START_LOC
cp $START_LOC $START_LOC_TRANSFORMS

echo "$INSTALL_DIR_LIBS/gryptonite"                 >> $START_LOC
echo "$INSTALL_DIR_LIBS/grypto_transforms"          >> $START_LOC_TRANSFORMS

chmod +x $START_LOC
chmod +x $START_LOC_TRANSFORMS

if [ -d "$INSTALL_DIR_LIBS" ]; then
    cp -r * "$INSTALL_DIR_LIBS"
    chmod 755 -R "$INSTALL_DIR_LIBS"
fi


# Show confirmation message
if [ -f "$START_LOC" ]; then
    echo "Gryptonite libraries installed to $INSTALL_DIR_LIBS"
    echo "Start application with:  $START_LOC"
fi
