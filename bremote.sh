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

GAME_SVR_REMOTE=`yq e '.game_server.remote_path' deploy.yaml`
if [ -z "$GAME_SVR_REMOTE" ]; then
    echo "Error: missing game server remote path in deploy.yaml" >&2
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

RED_USER=redsvcs
LOCAL_REMOTE_PATH=remote
REMOTE_SVC_FILE=remote.service
REMOTE_EXECUTABLE=start.sh
LOCAL_LAUNCHER_PATH=ctl_server
LAUNCHER_SVC_FILE=launcher.service
LAUNCHER_EXECUTABLE=start.sh
GENERATED_PATH=generated
BUILD_PATH="red_builds/${LOCAL_REMOTE_PATH}"

# Generate config files for remote
echo "Generating config files for remote..." >&2
./gen-config.py --config remote-config > ${LOCAL_REMOTE_PATH}/config.yaml
./gen-config.py --config remote-games > ${LOCAL_REMOTE_PATH}/games.yaml

# Generate config files for launcher
echo "Generating config files for launcher..." >&2
./gen-config.py --config launcher-config > ${LOCAL_LAUNCHER_PATH}/config.yaml
./gen-config.py --config launcher-games > ${LOCAL_LAUNCHER_PATH}/games.yaml

# Copy to build server
echo "Building protobuf files..." >&2
rsync -trph \
    --exclude '.*' \
    "${LOCAL_REMOTE_PATH}/" \
    "${BUILD_SVR}:${BUILD_PATH}/"

# Set up environment and compile protobufs on build server
ssh "${BUILD_SVR}" "cd ${BUILD_PATH} && ./build.sh"

# Copy generated code back to local
rsync -trph \
    "${BUILD_SVR}:${BUILD_PATH}/${GENERATED_PATH}" \
    "${LOCAL_REMOTE_PATH}/"
cp -r "${LOCAL_REMOTE_PATH}/${GENERATED_PATH}" "${LOCAL_LAUNCHER_PATH}/"

# Copy remote to game server
echo "Deploying remote to game server..." >&2
ssh "${GAME_SVR_HOST}" "mkdir -p \"${GAME_SVR_REMOTE}\""
rsync -trph \
    --exclude '.*' \
    --exclude 'build.sh' \
    --exclude 'proto/' \
    "${LOCAL_REMOTE_PATH}/" \
    "${GAME_SVR_HOST}:${GAME_SVR_REMOTE}"

ssh "${GAME_SVR_HOST}" "
    (
        test -d \"${GAME_SVR_REMOTE}/.venv\" 2>/dev/null ||
        (
            echo \"Setting up virtual environment...\" >&2 &&
            cd \"${GAME_SVR_REMOTE}\" &&
            python3 -m venv .venv &&
            source .venv/bin/activate &&
            pip install -r requirements.txt
        )
    ) &&
    (
        SYSTEMD_PATH=\"\${HOME}/.config/systemd/user\" &&
        SVC_PATH=\"\${SYSTEMD_PATH}/red_${REMOTE_SVC_FILE}\" &&
        test -f \"\${SVC_PATH}\" 2>/dev/null ||
        (
            echo \"Setting up remote systemd service...\" >&2 &&
            mkdir -p \"\${SYSTEMD_PATH}\" &&
            P=\$(readlink -f \"${GAME_SVR_REMOTE}/${REMOTE_EXECUTABLE}\") &&
            cat \"${GAME_SVR_REMOTE}/${REMOTE_SVC_FILE}\" | sed \"s|{SERVICE_PATH}|\${P}|g\" > \"\${SVC_PATH}\" &&
            systemctl --user daemon-reload &&
            systemctl --user enable \"red_${REMOTE_SVC_FILE}\" &&
            sudo loginctl enable-linger \"\${USER}\"
        )
    ) &&
    (
        echo \"Restarting remote service...\" >&2 &&
        systemctl --user restart \"red_${REMOTE_SVC_FILE}\"
    )
"

# Copy launcher to control server
echo "Deploying launcher to control server..." >&2
ssh "${CONTROL_SVR_HOST}" "mkdir -p \"${CONTROL_SVR_PATH}\""
rsync -trph \
    --exclude '.*' \
    --exclude '*.db' \
    --exclude '*.example' \
    --exclude '__pycache__' \
    "${LOCAL_LAUNCHER_PATH}/" \
    "${CONTROL_SVR_HOST}:${CONTROL_SVR_PATH}/"

ssh "${CONTROL_SVR_HOST}" "
    (
        test -d \"${CONTROL_SVR_PATH}/.venv\" 2>/dev/null ||
        (
            echo \"Setting up virtual environment...\" >&2 &&
            cd \"${CONTROL_SVR_PATH}\" &&
            python3 -m venv .venv &&
            source .venv/bin/activate &&
            pip install -r requirements.txt
        )
    ) &&
    (
        SYSTEMD_PATH=\"\${HOME}/.config/systemd/user\" &&
        SVC_PATH=\"\${SYSTEMD_PATH}/red_${LAUNCHER_SVC_FILE}\" &&
        test -f \"\${SVC_PATH}\" 2>/dev/null ||
        (
            echo \"Setting up launcher systemd service...\" >&2 &&
            mkdir -p \"\${SYSTEMD_PATH}\" &&
            P=\$(readlink -f \"${CONTROL_SVR_PATH}/${LAUNCHER_EXECUTABLE}\") &&
            cat \"${CONTROL_SVR_PATH}/${LAUNCHER_SVC_FILE}\" | sed \"s|{SERVICE_PATH}|\${P}|g\" > \"\${SVC_PATH}\" &&
            systemctl --user daemon-reload &&
            systemctl --user enable \"red_${LAUNCHER_SVC_FILE}\" &&
            sudo loginctl enable-linger \"\${USER}\"
        )
    ) &&
    (
        echo \"Restarting launcher service...\" >&2 &&
        systemctl --user restart \"red_${LAUNCHER_SVC_FILE}\"
    )
"
