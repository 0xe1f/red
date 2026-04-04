#!/bin/bash

## Copyright (c) 2024 Akop Karapetyan
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##    http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.

# This script builds specified game server projects and deploys them
# to a remote server.
# It's meant to be executed on the build server.

GAME_SVR_HOST="$1"
LOCAL_CORES_PATH="$2"
REMOTE_CORES_PATH="$3"

# Text formatting constants
BOLD_WHITE="$(tput bold)$(tput setaf 7)"
GREEN="$(tput setaf 2)"
RED="$(tput setaf 1)"
PLAIN="$(tput sgr0)"

if [ -z "${GAME_SVR_HOST}" ] || [ -z "${LOCAL_CORES_PATH}" ] || [ -z "${REMOTE_CORES_PATH}" ]; then
    echo -e "${RED}Error: missing arguments. Usage: $0 game_server_host local_cores_path remote_cores_path${PLAIN}" >&2
    exit 1
fi

cd `dirname $(readlink -f $0)`
shift 3

# Make sure at least one project is specified
ARGS="$@"
if [ ! -z "$ARGS" ]; then
    # Check that all specified directories have a build.sh
    for DIR in $ARGS; do
        if [ ! -d "$DIR" ]; then
            echo -e "${RED}Error: $DIR is not a valid module${PLAIN}" >&2
            exit 1
        fi
    done
fi

set -e

# Run build.sh in each specified directory
for DIR in $ARGS; do
    DIRNAME=$(basename $DIR)
    echo -e "${BOLD_WHITE}>> ${GREEN}Building $DIRNAME... ${PLAIN}"

    # Run build script and capture output
    BUILD_OUTPUT=$(./build_module.sh $DIRNAME | tee /dev/tty)
    # Get last line of output as files to copy
    FILES=$(echo "$BUILD_OUTPUT" | tail -n 1)

    pushd $DIRNAME > /dev/null

    if [[ "$FILES" == done:* ]]; then
        FILES="${FILES#done:}"
    else
        echo -e "${RED}Build failed ${PLAIN}" >&2
        exit 1
    fi

    echo -e "${BOLD_WHITE}>> ${GREEN}Installing... ${PLAIN}"
    # Create remote directory
    ssh "${GAME_SVR_HOST}" -t "mkdir -p ${REMOTE_CORES_PATH}/${DIRNAME}"
    # Copy built files to remote server
    rsync -trpKh \
        --info=progress2 \
        $FILES \
        "${GAME_SVR_HOST}:${REMOTE_CORES_PATH}/${DIRNAME}/"

    popd > /dev/null
done
