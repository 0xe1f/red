#!/bin/bash

## Game Host Query
## 2024, Akop Karapetyan

HOST=$1

if [ -z "${HOST}" ]; then
    echo "Missing game host" >&2
    exit
fi

ssh "${HOST}" "pgrep -a fbneo" | awk '{print $3}'
