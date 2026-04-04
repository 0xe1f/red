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

# This script stages projects for build on build server and starts build.
# It's meant to be executed on the local machine.

BUILD_SVR="$1"
GAME_SVR_HOST="$2"
LOCAL_CORES_PATH="$3"
REMOTE_CORES_PATH="$4"
BUILD_PATH="red_builds/${LOCAL_CORES_PATH}"

# Text formatting constants
BOLD_WHITE="$(tput bold)$(tput setaf 7)"
GREEN="$(tput setaf 2)"
RED="$(tput setaf 1)"
PLAIN="$(tput sgr0)"

if [ -z "${BUILD_SVR}" ] || [ -z "${GAME_SVR_HOST}" ] || [ -z "${LOCAL_CORES_PATH}" ] || [ -z "${REMOTE_CORES_PATH}" ]; then
    echo -e "${RED}Error: missing arguments. Usage: $0 build_server game_server_host local_cores_path remote_cores_path${PLAIN}" >&2
    exit 1
fi

cd `dirname $(readlink -f $0)`
shift 4

ARGS="$@"

# Check that all specified directories are present and valid
for DIR in $ARGS; do
    if [ ! -d "$DIR" ]; then
        echo -e "${RED}Error: $DIR is not a valid module${PLAIN}" >&2
        exit 1
    fi
done

set -e

copy_scripts() {
    echo -e "${BOLD_WHITE}>> ${GREEN}Copying common code... ${PLAIN}"
    rsync -tph \
        --info=progress2 \
        build_module.sh \
        build.sh \
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
        "${dirname}" \
        "${BUILD_SVR}:${BUILD_PATH}/"
}

# Create build directory on build server
ssh $BUILD_SVR -t "mkdir -p ${BUILD_PATH}"

# Copy common scripts/files to build server
copy_scripts

# Stage each project
for DIR in $ARGS; do
    stage_project $(basename $DIR)
done

# Start build on build server
ssh "${BUILD_SVR}" -t "${BUILD_PATH}/build.sh ${GAME_SVR_HOST} ${LOCAL_CORES_PATH} ${REMOTE_CORES_PATH} ${ARGS}"
