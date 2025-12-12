#!/bin/bash

cd `dirname $(readlink -f $0)`

APP_ID=`cat .launch_info 2> /dev/null | awk '{ print $1 }'`

if [ ! -z "$APP_ID" ]; then
    $APP_ID/stop.sh >> log.txt 2>&1
    rm -f .launch_info
fi
