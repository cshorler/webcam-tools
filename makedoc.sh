#!/bin/bash

set -e

export SRCDIR="."
export DOCDIR="./doxygen-output"
export CONFIG="doxygen/libwebcam.cfg"

echo "Cleaning up output directory '$DOCDIR' ..."
rm -rf $DOCDIR
echo -e "Done.\n"

echo "Generating Doxygen documentation using configuration file '$CONFIG' ..."
mkdir -p $DOCDIR
doxygen $CONFIG
echo -e "Done.\n"

echo "Copying extra resource files ..."
cp -v doxygen/res/* $DOCDIR/libwebcam/html/
echo -e "Done.\n"
