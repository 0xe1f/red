#!/bin/bash

## Game Host Query
## 2024, Akop Karapetyan

EMULATOR_EXE="fbneo"
HOST=$1

if [ -z "${HOST}" ]; then
    echo "Missing game host" >&2
    exit
fi

ssh "${HOST}" "pgrep -a ${EMULATOR_EXE}" | awk '{print $3}'
