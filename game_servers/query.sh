#!/bin/bash

cd `dirname $(readlink -f $0)`

APP_ID=
TITLE_ID=
if [ -f .launch_info ]; then
    LAUNCH_INFO=`cat .launch_info 2> /dev/null`
    APP_ID=`echo "$LAUNCH_INFO" | awk '{ print $1 }'`
    if [ -z "$APP_ID" ]; then
        echo "Cannot determine app_id (nothing running?)" >&2
        exit 1
    fi
    TITLE_ID=`echo "$LAUNCH_INFO" | awk '{ print $2 }'`
fi

VOL=`amixer sget PCM | grep '%]' | tail -1 | sed -n -E 's/.*\[([^%]+)%].*/\1/p'`

echo "$APP_ID $TITLE_ID $VOL"
