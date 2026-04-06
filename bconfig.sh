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

# Build and deploy configuration files and update launcher and remote

# Change to the script's directory
cd "$(dirname "$0")"

# Text formatting constants
BOLD_WHITE="$(tput bold)$(tput setaf 7)"
CLR_OK="$(tput setaf 2)"
CLR_WARN="$(tput setaf 3)"
CLR_ERR="$(tput setaf 1)"
CLR_RST="$(tput sgr0)"

set -e

# Verify prerequisites
if ! command -v yq &> /dev/null; then
    echo "${CLR_ERR}Error: yq is not installed${CLR_RST}" >&2
    exit 1
fi

GAME_SVR_HOST=`yq e '.game_server.hostname' deploy.yaml`
if [ -z "$GAME_SVR_HOST" ]; then
    echo "${CLR_ERR}Error: missing game server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

GAME_SVR_REMOTE=`yq e '.game_server.remote_path' deploy.yaml`
if [ -z "$GAME_SVR_REMOTE" ]; then
    echo "${CLR_ERR}Error: missing game server remote path in deploy.yaml${CLR_RST}" >&2
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

LOCAL_REMOTE_PATH=remote
LOCAL_LAUNCHER_PATH=launcher

# Need to activate virtual environment to run gen-config.py
if [ ! -d ".venv" ]; then
    echo "${CLR_WARN}Setting up virtual environment to generate config...${CLR_RST}" >&2
    python3 -m venv .venv
fi
source .venv/bin/activate

# Generate config files for remote
echo "${CLR_OK}Generating config files...${CLR_RST}" >&2
./gen-config.py --config remote-games > ${LOCAL_REMOTE_PATH}/games.yaml
./gen-config.py --config launcher-games > ${LOCAL_LAUNCHER_PATH}/games.yaml

# Copy remote to game server
echo "${CLR_OK}Updating remote's configuration...${CLR_RST}" >&2
rsync -tph \
    "${LOCAL_REMOTE_PATH}/games.yaml" \
    "${GAME_SVR_HOST}:${GAME_SVR_REMOTE}"

# Copy launcher to control server
echo "${CLR_OK}Updating launcher's configuration...${CLR_RST}" >&2
rsync -tph \
    "${LOCAL_LAUNCHER_PATH}/games.yaml" \
    "${CONTROL_SVR_HOST}:${CONTROL_SVR_PATH}/"

echo "${CLR_OK}All done.${CLR_RST}" >&2
