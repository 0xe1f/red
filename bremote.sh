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

# Change to the script's directory
cd "$(dirname "$0")"

if ! command -v yq &> /dev/null; then
    echo "${CLR_ERR}Error: yq is not installed${CLR_RST}" >&2
    exit 1
fi

GAME_SVR_HOST=`yq e '.game_server.hostname' deploy.yaml`
if [ -z "$GAME_SVR_HOST" ]; then
    echo "Error: missing game server hostname in deploy.yaml" >&2
    exit 1
fi

CONTROL_SVR_HOST=`yq e '.control_server.hostname' deploy.yaml`
if [ -z "$CONTROL_SVR_HOST" ]; then
    echo "${CLR_ERR}Error: missing control server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

CONTROL_SVR_PATH=`yq e '.control_server.path' deploy.yaml`
if [ -z "$CONTROL_SVR_PATH" ]; then
    echo "${CLR_ERR}Error: missing control server path in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

BUILD_SVR=`yq e '.control_server.hostname' deploy.yaml`
if [ -z "$BUILD_SVR" ]; then
    echo "${CLR_ERR}Error: missing build server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

APP_PATH=remote
BUILD_PATH="red_builds/${APP_PATH}"

# Copy to build server
rsync -trph \
    --info=progress2 \
    --exclude '.*' \
    "${APP_PATH}/" \
    "${BUILD_SVR}:${BUILD_PATH}/"

# Set up environment and compile protobufs on build server
ssh "${BUILD_SVR}" "cd ${BUILD_PATH} && ./build.sh"

# Copy generated code back to local
rsync -trph \
    --info=progress2 \
    "${BUILD_SVR}:${BUILD_PATH}/generated" \
    "${APP_PATH}/"

# Copy to game server
rsync -trph \
    --info=progress2 \
    --exclude '.*' \
    --exclude 'setup.sh' \
    --exclude 'proto/' \
    "${APP_PATH}/" \
    "${GAME_SVR_HOST}:${APP_PATH}/"

# Copy to control server
rsync -trph \
    --info=progress2 \
    --exclude '.*' \
    --exclude 'setup.sh' \
    --exclude 'proto/' \
    "${APP_PATH}/generated" \
    "${CONTROL_SVR_HOST}:${CONTROL_SVR_PATH}/"

# FIXME - copy configs
