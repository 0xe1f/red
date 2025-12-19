#!/bin/bash

HOST="$1"
DEST="$2"

if [ -z "$HOST" ] || [ -z "$DEST" ]; then
    echo "Error: missing arguments. Usage: $0 host dest_path" >&2
    exit 1
fi

cd `dirname $(readlink -f $0)`

set -e
rsync -vt * $HOST:$DEST

for DIR in */; do
    echo "Building ${DIR%/}..."
    $DIR/build.sh $HOST $DEST
done
