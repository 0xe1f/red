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

# Stages, builds and deploys video clients to the hosts

# Text formatting constants
BOLD_WHITE="$(tput bold)$(tput setaf 7)"
CLR_OK="$(tput setaf 2)"
CLR_WARN="$(tput setaf 3)"
CLR_ERR="$(tput setaf 1)"
CLR_RST="$(tput sgr0)"

# Check prerequisites
if [ ! -f "deploy.yaml" ]; then
    echo "${CLR_ERR}Error: deploy.yaml not found${CLR_RST}" >&2
    exit 1
elif ! command -v yq &> /dev/null; then
    echo "${CLR_ERR}Error: yq is not installed${CLR_RST}" >&2
    exit 1
fi

BUILD_SVR=`yq e '.control_server.hostname' deploy.yaml`
if [ -z "$BUILD_SVR" ]; then
    echo "${CLR_ERR}Error: missing build server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

CLIENT_COUNT=`yq e ".game_clients | length - 1" deploy.yaml`
if [ $CLIENT_COUNT -le 0 ] ; then
    echo "${CLR_ERR}Error: missing game client config in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

APP_PATH=pubsub
BUILD_PATH="red_builds/${APP_PATH}"
TARGET=$1

shift
set -e

echo -e "${BOLD_WHITE}>> ${CLR_OK}Staging build... ${CLR_RST}"
rsync -trph \
    --info=progress2 \
    --exclude '.*' \
    "${APP_PATH}/" \
    "${BUILD_SVR}:${BUILD_PATH}/"

echo -e "${BOLD_WHITE}>> ${CLR_OK}Building... ${CLR_RST}"
ssh $BUILD_SVR -t "cd ${BUILD_PATH} && make ${TARGET}"

for CLIENT in `seq 0 $CLIENT_COUNT`; do
    CL_HOST=`yq e ".game_clients[$CLIENT].hostname" deploy.yaml`
    CL_PATH=`yq e ".game_clients[$CLIENT].path" deploy.yaml`
    CL_EXE=`yq e ".game_clients[$CLIENT].exe" deploy.yaml`

    echo -e "${BOLD_WHITE}>> ${CLR_OK}Deploying to ${CL_HOST}... ${CLR_RST}"
    ssh $CL_HOST -t "mkdir -p ${CL_PATH}"
    set +e
    if ! scp "${BUILD_SVR}:${BUILD_PATH}/${CL_EXE}" "${CL_HOST}:${CL_PATH}/"; then
        echo -e "${CLR_WARN}Warning: failed to deploy sub to ${CL_HOST}${CLR_RST}" >&2
    fi
    set -e
done

GAME_SVR_HOST=`yq e '.game_server.hostname' deploy.yaml`
if [ -z "$GAME_SVR_HOST" ]; then
    echo "${CLR_ERR}Error: missing game server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

GAME_SVR_PATH=`yq e '.game_server.path' deploy.yaml`
if [ -z "$GAME_SVR_PATH" ]; then
    echo "${CLR_ERR}Error: missing game server path in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

set +e
if ! scp "${BUILD_SVR}:${BUILD_PATH}/pub" "${GAME_SVR_HOST}:${GAME_SVR_PATH}/"; then
    echo -e "${CLR_WARN}Warning: failed to deploy pub to ${GAME_SVR_HOST}${CLR_RST}" >&2
fi
set -e

echo -e "${BOLD_WHITE}>> ${CLR_OK}All done... ${CLR_RST}"
