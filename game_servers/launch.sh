#!/bin/bash

EMULATOR_DIR=fbneo
EMULATOR_EXE=fbneo

cd `dirname $0`
killall -s SIGINT ${EMULATOR_EXE} 2> /dev/null

if [ "$1" == "--stop" ]; then
    exit 0
fi

cd ${EMULATOR_DIR}
set -o pipefail
./${EMULATOR_EXE} $@ 2>&1 | tee -a log.txt
