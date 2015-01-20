#! /bin/bash

LIBDIR="/usr/local/lib/gryptonite"

if [ -f /usr/local/bin/gryptonite ]; then
	rm /usr/local/bin/gryptonite;
fi;

if [ -d "$LIBDIR" ]; then
	rm -rf "$LIBDIR";
fi;

