#!/bin/bash

if [[ -z "$THERING_HOME" || ! -d "$THERING_HOME" ]]; then
	echo "THERING_HOME variable is not set"
	exit 1
fi
if [[ ! -r "$THERING_HOME/thering.conf" ]]; then
	echo "THERING_HOME not set properly. Cannot find thering.conf"
	exit 1
fi

. $THERING_HOME/thering.conf

echo "Setting up with THERING_HOME=$THERING_HOME"

mkdir -p $DBDIR

if [ -f $DBFILE ]; then
	echo "Database $DBFILE already exists!"
else
	$SQLITE $DBFILE ".read image_keys.sql"
	chmod 644 $DBFILE
fi

mkdir -p -m 755 $ENCLIB
mkdir -p -m 755 $ENCBIN
