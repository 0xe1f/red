#!/bin/bash

# FIXME: hardcoded for fbneo for now
EMULATOR_EXE=fbneo

VOL=`amixer sget PCM | grep '%]' | tail -1 | sed -n -E 's/.*\[([^%]+)%].*/\1/p'`
TITLE=`pgrep -a $EMULATOR_EXE | awk '{print $3}'`

echo "$TITLE $VOL"
