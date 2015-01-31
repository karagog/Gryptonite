#! /bin/bash

# You cannot change this location, it is compiled into the application
INSTALL_DIR_LIBS="/usr/local/lib/gryptonite"

START_LOC="/usr/local/bin/gryptonite"
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

cd $SCRIPT_DIR

# Create an uninstall script
echo "#! /bin/bash"                 > uninstall.bash
echo "rm $START_LOC"                >> uninstall.bash
echo "rm -rf $INSTALL_DIR_LIBS"     >> uninstall.bash
chmod +x uninstall.bash

# Create a startup script
echo "#! /bin/bash"                                 > $START_LOC
#echo "export QT_PLUGIN_PATH=$INSTALL_DIR_LIBS"      >> $START_LOC
echo "export LD_LIBRARY_PATH=$INSTALL_DIR_LIBS"      >> $START_LOC
echo "$INSTALL_DIR_LIBS/gryptonite"                 >> $START_LOC
chmod +x $START_LOC

# Create the installation directory
if [ ! -d "$INSTALL_DIR_LIBS" ]; then
    mkdir "$INSTALL_DIR_LIBS";
fi;


if [ -d "$INSTALL_DIR_LIBS" ]; then
    cp -r * "$INSTALL_DIR_LIBS";
    chmod 755 -R "$INSTALL_DIR_LIBS";
fi;


# Show confirmation message
if [ -f "$START_LOC" ]; then
    echo "Gryptonite libraries installed to $INSTALL_DIR_LIBS"
    echo "Start application with:  $START_LOC"
fi;
