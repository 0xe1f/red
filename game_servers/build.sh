#!/bin/bash

HOST="$1"
DEST="$2"

if [ -z "$HOST" ] || [ -z "$DEST" ]; then
    echo "Error: missing arguments. Usage: $0 host dest_path" >&2
    exit 1
fi

cd "$(dirname "$0")"

set -e
rsync -vt * $HOST:$DEST

echo "Building fbneo" >&2
cd fbneo
make ss RELEASEBUILD=1 BUILD_NATIVE=1
ssh -o "StrictHostKeyChecking no" $HOST -t "mkdir -p $DEST/fbneo"
scp -o "StrictHostKeyChecking no" fbneo $HOST:$DEST/fbneo/

cd ..
