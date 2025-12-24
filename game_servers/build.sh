#!/bin/bash

# This script builds specified game server projects and deploys them
# to a remote server.
# It's meant to be executed on the build server.

GAME_SVR_HOST="$1"
GAME_SVR_PATH="$2"

# Text formatting constants
BOLD_WHITE="$(tput bold)$(tput setaf 7)"
GREEN="$(tput setaf 2)"
RED="$(tput setaf 1)"
PLAIN="$(tput sgr0)"

if [ -z "$GAME_SVR_HOST" ] || [ -z "$GAME_SVR_PATH" ]; then
    echo -e "${RED}Error: missing arguments. Usage: $0 game_server_host game_server_path${PLAIN}" >&2
    exit 1
fi

cd `dirname $(readlink -f $0)`
shift 2

# Make sure at least one project is specified
ARGS="$@"
if [ -z "$ARGS" ]; then
    echo -e "${RED}Error: No projects specified for build.${PLAIN}" >&2
    exit 1
else
    # Check that all specified directories have a build.sh
    for DIR in $ARGS; do
        if [ ! -f "$DIR/build.sh" ]; then
            echo -e "${RED}Error: No build.sh found in $DIR${PLAIN}" >&2
            exit 1
        fi
    done
fi

# Deploy common scripts to game server
set -e
rsync -tph \
    --info=progress2 \
    --exclude='*/' \
    --exclude='build.sh' \
    * \
    $GAME_SVR_HOST:$GAME_SVR_PATH

# Run build.sh in each specified directory
for DIR in $ARGS; do
    DIRNAME=$(basename $DIR)
    echo -e "${BOLD_WHITE}>> ${GREEN}Building $DIRNAME... ${PLAIN}"

    pushd $DIRNAME > /dev/null

    # Run build script and capture output
    BUILD_OUTPUT=$(./build.sh | tee /dev/tty)
    # Get last line of output as files to copy
    FILES=$(echo "$BUILD_OUTPUT" | tail -n 1)

    echo -e "${BOLD_WHITE}>> ${GREEN}Installing... ${PLAIN}"
    # Create remote directory
    ssh -o "StrictHostKeyChecking no" $GAME_SVR_HOST -t "mkdir -p $GAME_SVR_PATH/$DIRNAME"
    # Copy built files to remote server
    rsync -trpKh \
        --info=progress2 \
        $FILES launch.sh stop.sh \
        "$GAME_SVR_HOST:$GAME_SVR_PATH/$DIRNAME/"

    popd > /dev/null
done
