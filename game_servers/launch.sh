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

echo "Launching $APP_ID/$TITLE_ID..." >> log.txt

set -o pipefail
$APP_ID/launch.sh $@ 2>&1 | tee -a log.txt

echo "$APP_ID $TITLE_ID" > $BASE_DIR/.launch_info
