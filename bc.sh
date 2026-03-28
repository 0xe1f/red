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
GREEN="$(tput setaf 2)"
RED="$(tput setaf 1)"
PLAIN="$(tput sgr0)"

# Check prerequisites
if [ ! -f "deploy.yaml" ]; then
    echo "${RED}Error: deploy.yaml not found${PLAIN}" >&2
    exit 1
elif ! command -v yq &> /dev/null; then
    echo "${RED}Error: yq is not installed${PLAIN}" >&2
    exit 1
fi

BUILD_SVR=`yq e '.control_server.hostname' deploy.yaml`
if [ -z "$BUILD_SVR" ]; then
    echo "${RED}Error: missing build server hostname in deploy.yaml${PLAIN}" >&2
    exit 1
fi

CLIENT_COUNT=`yq e ".game_clients | length - 1" deploy.yaml`
if [ $CLIENT_COUNT -le 0 ] ; then
    echo "${RED}Error: missing game client config in deploy.yaml${PLAIN}" >&2
    exit 1
fi

APP_PATH=pubsub
BUILD_PATH="red_builds/${APP_PATH}"
TARGET=$1

shift
set -e

echo -e "${BOLD_WHITE}>> ${GREEN}Staging build... ${PLAIN}"
rsync -trph \
    --info=progress2 \
    --exclude '.*' \
    "${APP_PATH}/" \
    "${BUILD_SVR}:${BUILD_PATH}/"

echo -e "${BOLD_WHITE}>> ${GREEN}Building... ${PLAIN}"
ssh $BUILD_SVR -t "cd ${BUILD_PATH} && make $TARGET"

for CLIENT in `seq 0 $CLIENT_COUNT`; do
    CL_HOST=`yq e ".game_clients[$CLIENT].hostname" deploy.yaml`
    CL_PATH=`yq e ".game_clients[$CLIENT].path" deploy.yaml`
    CL_EXE=`yq e ".game_clients[$CLIENT].exe" deploy.yaml`

    echo -e "${BOLD_WHITE}>> ${GREEN}Deploying to ${CL_HOST}... ${PLAIN}"
    ssh $CL_HOST -t "mkdir -p ${CL_PATH}"
    scp "${BUILD_SVR}:${BUILD_PATH}/${CL_EXE}" "${CL_HOST}:${CL_PATH}/"
done

# scp "${BUILD_SVR}:${BUILD_PATH}/pub" "${CL_HOST}:${CL_PATH}/"

echo -e "${BOLD_WHITE}>> ${GREEN}All done... ${PLAIN}"
