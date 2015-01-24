#! /bin/bash

TOP_DIR=../..
QT_DIR=/opt/Qt/5.4/gcc_64
REPO_DIR=gryptonite_v$1

mkdir $REPO_DIR

# Copy gryptonite binaries
cp $TOP_DIR/bin/*               $REPO_DIR
cp $TOP_DIR/lib/*.so.3            $REPO_DIR

# Copy GUtil libs
cp $TOP_DIR/gutil/lib/libGUtil.so.0             $REPO_DIR
cp $TOP_DIR/gutil/lib/libGUtilQt.so.0           $REPO_DIR
cp $TOP_DIR/gutil/lib/libGUtilCryptoPP.so.0     $REPO_DIR
cp $TOP_DIR/gutil/lib/libGUtilAboutPlugin.so    $REPO_DIR

# Copy Qt libs
cp $QT_DIR/lib/libQt5Core.so.5                  $REPO_DIR
cp $QT_DIR/lib/libQt5Gui.so.5                   $REPO_DIR
cp $QT_DIR/lib/libQt5Widgets.so.5               $REPO_DIR
cp $QT_DIR/lib/libQt5Sql.so.5                   $REPO_DIR
cp $QT_DIR/lib/libQt5Xml.so.5                   $REPO_DIR
cp $QT_DIR/lib/libicui18n.so.53                 $REPO_DIR
cp $QT_DIR/lib/libicuuc.so.53                   $REPO_DIR
cp $QT_DIR/lib/libicudata.so.53                 $REPO_DIR

# Copy Qt plugins
mkdir $REPO_DIR/sqldrivers
mkdir $REPO_DIR/platforms
cp $QT_DIR/plugins/sqldrivers/libqsqlite.so     $REPO_DIR/sqldrivers
cp $QT_DIR/plugins/platforms/*                  $REPO_DIR/platforms

# Copy install scripts
cp install.bash     $REPO_DIR
chmod +x $REPO_DIR/install.bash

tar -czf $REPO_DIR.tar.gz $REPO_DIR
