#! /bin/bash

LIBDIR="/usr/local/lib/gryptonite"

if [ ! -f /usr/local/bin/gryptonite ]; then
	cp gryptonite /usr/local/bin;

	chmod 755 /usr/local/bin/gryptonite
fi;

if [ ! -d "$LIBDIR" ]; then
	mkdir "$LIBDIR";
fi;

if [ -d "$LIBDIR" ]; then
	cp libQt*.so.4 "$LIBDIR";

	chmod 755 -R "$LIBDIR";
fi;

