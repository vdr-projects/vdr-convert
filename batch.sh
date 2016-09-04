#!/bin/bash
# Shell script to convert VDR recordings in todo.txt to mpeg4 files. No upload
#------------------------------------------------------------------------------
# Revision History
#------------------------------------------------------------------------------
#
# $Log: batch.sh,v $
# Revision 1.3  2016/09/01 13:08:06  richard
# Pass multiple vdr-convert flags
# Update VDR afterwards
#
# Revision 1.2  2014/12/11 15:24:41  richard
# Improvements
#
# Revision 1.1  2014/12/10 10:44:19  richard
# Initial import - as used in BKK 2014
#
#------------------------------------------------------------------------------

# Usual way to create a "todo" list is as follows:
# find $DIR -type d | grep -i '.rec$' > $DIR/todo.txt

#Default directory where we work
input='/mnt/lvm/TV'
root=$input

function usage {
    echo "usage: $0 <vdr-convert args>"
}

function logit() {
  logger -s -p local2.warn -t batch "$1"
}

#------------------------------------------------------------------------------
# Start of script
#------------------------------------------------------------------------------

[ $# -eq 0 ] && usage && exit 1

for arg in "$@";
do
  args="$args '$arg'"
done

# Timeout req'd for troublesome conversions where ffmpeg sometimes get stuck. 
# (NOTE Causes issues when run interactively.)
NAMES="$(< $(pwd)/todo.txt)" #names from todo.txt file in this directory
for NAME in $NAMES; do
#   /bin/su vdr -c vdr-convert -i "\"$NAME"\" $args"
  timeout -k 5h 4h sh -c "vdr-convert -i "\"$NAME"\" $args"
  [ $? -ne 0 ] &&  logit "Fail: problem converting $NAME"
done

#Ask VDR to re-read the files
touch "$root/.update"

# --------- $Id: batch.sh,v 1.3 2016/09/01 13:08:06 richard Exp $ ---------- END
