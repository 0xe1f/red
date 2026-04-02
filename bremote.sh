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

# Build and deploy launcher to control server and remote to game server

# Change to the script's directory
cd "$(dirname "$0")"

set -e

# Verify prerequisites
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

REMOTE_PATH=remote
GENERATED_PATH=generated
BUILD_PATH="red_builds/${REMOTE_PATH}"

# Generate config files for remote
echo "Generating config files for remote..." >&2
./gen-config.py --config remote-config > ${REMOTE_PATH}/config.yaml
./gen-config.py --config remote-games > ${REMOTE_PATH}/games.yaml

# Generate config files for launcher
echo "Generating config files for launcher..." >&2
./gen-config.py --config launcher-config > ${CONTROL_SVR_PATH}/config.yaml
./gen-config.py --config launcher-games > ${CONTROL_SVR_PATH}/games.yaml

# Copy to build server
echo "Building protobuf files..." >&2
rsync -trph \
    --exclude '.*' \
    "${REMOTE_PATH}/" \
    "${BUILD_SVR}:${BUILD_PATH}/"

# Set up environment and compile protobufs on build server
ssh "${BUILD_SVR}" "cd ${BUILD_PATH} && ./build.sh"

# Copy generated code back to local
rsync -trph \
    "${BUILD_SVR}:${BUILD_PATH}/${GENERATED_PATH}" \
    "${REMOTE_PATH}/"
cp -r "${REMOTE_PATH}/${GENERATED_PATH}" "${CONTROL_SVR_PATH}/"

# Copy remote to game server
echo "Deploying remote to game server..." >&2
rsync -trph \
    --exclude '.*' \
    --exclude 'build.sh' \
    --exclude 'proto/' \
    "${REMOTE_PATH}" \
    "${GAME_SVR_HOST}:"

# Copy launcher to control server
echo "Deploying launcher to control server..." >&2
rsync -trph \
    --exclude '.*' \
    --exclude '*.db' \
    --exclude '*.example' \
    --exclude '__pycache__' \
    "${CONTROL_SVR_PATH}" \
    "${CONTROL_SVR_HOST}:"

# FIXME: launch launcher and remote
# f'cd {config.control_server.path}; nohup ./{CTL_SERVER_EXE} >log.txt 2>&1 &'
