#!/bin/bash

# This script stages projects for build on build server and starts build.
# It's meant to be executed on the local machine.

BUILD_SVR="$1"
GAME_SVR_HOST="$2"
GAME_SVR_PATH="$3"
BUILD_PATH="red_builds/${GAME_SVR_PATH}"

# Text formatting constants
BOLD_WHITE="$(tput bold)$(tput setaf 7)"
GREEN="$(tput setaf 2)"
RED="$(tput setaf 1)"
PLAIN="$(tput sgr0)"

if [ -z "$BUILD_SVR" ] || [ -z "$GAME_SVR_HOST" ] || [ -z "$GAME_SVR_PATH" ]; then
    echo -e "${RED}Error: missing arguments. Usage: $0 build_server game_server_host game_server_path${PLAIN}" >&2
    exit 1
fi

cd `dirname $(readlink -f $0)`
shift 3

# If no projects are specified, stage all directories
ARGS="$@"
if [ -z "$ARGS" ]; then
    ARGS=$(find . -maxdepth 1 -type d -not -name '.*' | tr -s '[:space:]' ' ')
fi

# Check that all specified directories have a build.sh
for DIR in $ARGS; do
    if [ ! -f "$DIR/build.sh" ]; then
        echo -e "${RED}Error: No build.sh found in $DIR${PLAIN}" >&2
        exit 1
    fi
done

set -e

copy_scripts() {
    echo -e "${BOLD_WHITE}>> ${GREEN}Copying scripts/files... ${PLAIN}"
    rsync -tph \
        --info=progress2 \
        --exclude '*.exclude' \
        --exclude 'stage.sh' \
        --exclude='*/' \
        ./* \
        "$BUILD_SVR:$BUILD_PATH/"
}

stage_project() {
    local dirname="$1"

    echo -e "${BOLD_WHITE}>> ${GREEN}Staging build for $dirname... ${PLAIN}"
    EXCLUDE_FILE=""
    if [ -f "${dirname}.exclude" ]; then
        EXCLUDE_FILE="--exclude-from ${dirname}.exclude"
    fi
    rsync -trph \
        --info=progress2 \
        --exclude-from common.exclude \
        $EXCLUDE_FILE \
        "$dirname" \
        "$BUILD_SVR:$BUILD_PATH/"
}

# Create build directory on build server
ssh -o "StrictHostKeyChecking no" $BUILD_SVR -t "mkdir -p $BUILD_PATH"

# Copy common scripts/files to build server
copy_scripts

# Stage each project
for DIR in $ARGS; do
    stage_project $(basename $DIR)
done

# Start build on build server
ssh -o "StrictHostKeyChecking no" $BUILD_SVR -t "$BUILD_PATH/build.sh $GAME_SVR_HOST $GAME_SVR_PATH $ARGS"
