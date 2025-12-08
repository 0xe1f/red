#!/bin/bash

APP_ID=$1
TITLE_ID=$2

if [ -z "$APP_ID" ] || [ -z "$TITLE_ID" ]; then
    echo "Error: app_id or title_id is missing" >&2
    exit 1
fi

cd `dirname $0`
./stopall.sh

# Discard the app_id
shift

cd "${APP_ID}"
set -o pipefail
./${APP_ID} $@ 2>&1 | tee -a log.txt
