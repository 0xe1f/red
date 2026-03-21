#!/bin/bash

APP_ID=$1
TITLE_ID=$2

if [ -z "$APP_ID" ] || [ -z "$TITLE_ID" ]; then
    echo "Error: app_id or title_id is missing" >&2
    exit 1
fi

BASE_DIR=`dirname $(readlink -f $0)`
cd "$BASE_DIR"

./stopall.sh

# Discard the app_id
shift
set -o pipefail

echo "Launching $APP_ID/$TITLE_ID..." >> log.txt

SO_FILE=`find $APP_ID/ -maxdepth 1 -type f -name '*_libretro.so'`
if [ -f "$SO_FILE" ]; then
    echo "Found core: $SO_FILE..." >> log.txt
    ROM_PATH="$(find -L $APP_ID/roms -maxdepth 1 -type f -name "$TITLE_ID.*" | head -1)"
    shift
    if [ -z "$ROM_PATH" ]; then
        echo "No ROM file found for $TITLE_ID" >> log.txt
        exit 1
    fi
    echo ./rh -c "$SO_FILE" "$ROM_PATH" "$@" >> log.txt
    ./rh -c "$SO_FILE" "$ROM_PATH" "$@" 2>&1 >> log.txt
else
    echo "Calling launch.sh..." >> log.txt
    $APP_ID/launch.sh $@ 2>&1 | tee -a log.txt
fi

echo "$APP_ID $TITLE_ID" > $BASE_DIR/.launch_info
