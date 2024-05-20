#!/bin/bash

EMULATOR_DIR=fbneo
EMULATOR_EXE=fbneo

cd `dirname $0`
killall -s SIGINT ${EMULATOR_EXE} 2> /dev/null

cd ${EMULATOR_DIR}
nohup ./${EMULATOR_EXE} $@ >log.txt 2>&1 &
