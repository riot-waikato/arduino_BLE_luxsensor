#!/bin/bash
# Sets DEBUGGING and device name in the given source code input file and writes to the
# output file.  The input file MUST be different than the output file otherwise the
# script will exit.
#
# The source code must already contain a line that matches:
#
#	#define DEBUG
#
# as this script will not add it.  Similarly, if a device name is specified a line that
# matches:
#
#	#define DEVICE
#
# must already exist.

# debugging
#set -x
#PS4='$LINENO: '

usage () {
	echo "Usage: setup.sh -i <input file> -o <output file> [DEBUG] [<device name>]";
}

FILE_IN=""
FILE_OUT=""

DEBUG=0
DEBUGFLAG=0
DEVICENAME=""
DEVICENAMEFLAG=0

# parse args
until [ -z "$1" ]
do
	# debug arg
	if [ "$1" == "DEBUG" ]
	then
		if [ $DEBUGFLAG -eq 1 ]
		then
			echo "Too many DEBUG arguments."
			exit 1
		fi
		DEBUG=1
		DEBUGFLAG=1
	# input file arg
	elif [ "$1" == "-i" ]
	then
		shift
		FILE_IN="$1"
	elif [ "$1" == "-o" ]
	then
		shift
		FILE_OUT="$1"
	else
		# device name arg
		if [ $DEVICENAMEFLAG -eq 1 ]
		then
			echo "Too many arguments."
			exit 1
		fi
		DEVICENAMEFLAG=1
		DEVICENAME="$1"
	fi
	shift
done

# check files are valid
if [ -z "$FILE_IN" -o -z "$FILE_OUT" ]
then
	echo "Need input and output files."
	usage
	exit 1
fi

if [ "$FILE_OUT" == "$FILE_IN" ]
then
	# don't remove output file if same as input file
	# TODO: handle properly
	exit
else
	rm "$FILE_OUT"
fi

echo "DEBUG=$DEBUG"
echo "DEVICENAME=$DEVICENAME"

if [ $DEBUG -eq 1 ]
then
	echo "Defining DEBUG..."
	SEDCOMMAND="-e /.*#define DEBUG.*/s/.*#define DEBUG.*/#define DEBUG/"
else
	# comment line out
	echo "Undefining DEBUG..."
	SEDCOMMAND="-e /.*#define DEBUG.*/s/.*#define DEBUG.*/\/\/#define DEBUG/"
	#sed -e '/.*#define DEBUG.*/s/.*#define DEBUG/\/\/#define DEBUG/' "$FILE_IN" >> "$FILE_OUT"
fi

# operate on input file
sed "$SEDCOMMAND" "$FILE_IN" >> $FILE_OUT

# check validity of device name
if [ $DEVICENAMEFLAG -eq 1 -a ! -z "$DEVICENAME" ]
then
	echo "Setting device name..."
	SEDCOMMAND="-e /^.*#define DEVICE .*/s/^.*#define DEVICE \".*\"/#define DEVICE \"$DEVICENAME\"/"

	# operate directly on output file, remove temp file
	sed -i.debug "$SEDCOMMAND" "$FILE_OUT"
	rm "$FILE_OUT".debug
else
	echo "Not changing device name..."
fi

exit
