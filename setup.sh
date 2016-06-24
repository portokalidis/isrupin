#!/bin/bash

if [[ -z "$ISR_HOME" || ! -d "$ISR_HOME" ]]; then
	echo "ISR_HOME variable is not set"
	exit 1
fi
if [[ ! -r "$ISR_HOME/isr.conf" ]]; then
	echo "ISR_HOME not set properly. Cannot find isr.conf"
	exit 1
fi

. $ISR_HOME/isr.conf

echo "Setting up with ISR_HOME=$ISR_HOME"

mkdir -p $DBDIR

if [ -f $DBFILE ]; then
	echo "Database $DBFILE already exists!"
else
	$SQLITE $DBFILE ".read image_keys.sql"
	chmod 644 $DBFILE
fi

mkdir -p -m 755 $ENCLIB
mkdir -p -m 755 $ENCBIN
