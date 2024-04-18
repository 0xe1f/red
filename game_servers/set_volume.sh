#!/bin/bash

VOLUME=$1

if [ ! "$1" -eq "$1" ]; then
    echo "Volume not valid" >&2
    exit 1
fi

if [ "$VOLUME" -lt 0 ] || [ "$VOLUME" -gt 100 ]; then
    echo "Volume out of range" >&2
    exit 1
fi

amixer sset PCM "$VOLUME"'%'
