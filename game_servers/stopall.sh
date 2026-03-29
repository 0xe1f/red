#!/bin/bash

cd `dirname $(readlink -f $0)`

PUB_PID=`pgrep -x pub`
if [ ! -z "$PUB_PID" ]; then
    echo "Stopping retrohost (pid $PUB_PID)..." >> log.txt
    kill -s SIGINT $PUB_PID
    for I in {1..8}; do
        if ! pgrep -x pub > /dev/null; then
            break
        fi
        sleep 0.25
    done
    if pgrep -x pub > /dev/null; then
        echo "Force-stopping..." >> log.txt
        kill -9 $PUB_PID
    fi
    rm -f .launch_info
else
    APP_ID=`cat .launch_info 2> /dev/null | awk '{ print $1 }'`
    if [ ! -z "$APP_ID" ]; then
        echo "Stopping with stop.sh..." >> log.txt
        $APP_ID/stop.sh >> log.txt 2>&1
        rm -f .launch_info
    fi
fi
