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

# Stages, builds and deploys publisher to game server host

# Change to the script's directory
cd "$(dirname "$0")"

set -e

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

BUILD_SVR=`yq e '.common.build_host' deploy.yaml`
if [ -z "${BUILD_SVR}" ]; then
    echo "${CLR_ERR}Error: missing build server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

GAME_SVR_HOST=`yq e '.game_server.hostname' deploy.yaml`
if [ -z "${GAME_SVR_HOST}" ]; then
    echo "${CLR_ERR}Error: missing game server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

CORES_PATH=`yq e '.game_server.cores_path' deploy.yaml`
if [ -z "${CORES_PATH}" ]; then
    echo "${CLR_ERR}Error: missing cores path in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

LOCAL_APP_PATH=pubsub
BUILD_PATH="red_builds/${LOCAL_APP_PATH}"
TARGET=pub

# Stage build
echo -e "${BOLD_WHITE}>> ${CLR_OK}Staging build... ${CLR_RST}"
ssh "${BUILD_SVR}" "mkdir -p \"${BUILD_PATH}\""
rsync -trph \
    --info=progress2 \
    --exclude '.*' \
    "${LOCAL_APP_PATH}/" \
    "${BUILD_SVR}:${BUILD_PATH}"

# Build on build server
echo -e "${BOLD_WHITE}>> ${CLR_OK}Building... ${CLR_RST}"
ssh "${BUILD_SVR}" "cd ${BUILD_PATH} && make ${TARGET}"

# Copy the generated executable locally
TEMP_FILE=`mktemp`
rsync -trph \
    "${BUILD_SVR}:${BUILD_PATH}/${TARGET}" \
    "${TEMP_FILE}"

# Deploy to game server host
echo -e "${BOLD_WHITE}>> ${CLR_OK}Deploying... ${CLR_RST}"
ssh "${GAME_SVR_HOST}" "mkdir -p \"${CORES_PATH}\""
rsync -trph \
    --info=progress2 \
    "${TEMP_FILE}" \
    "${GAME_SVR_HOST}:${CORES_PATH}/${TARGET}"

echo -e "${BOLD_WHITE}>> ${CLR_OK}All done... ${CLR_RST}"



# APP_PATH=pubsub
# BUILD_PATH="red_builds/${APP_PATH}"
# TARGET=$1

# shift
# set -e

# echo -e "${BOLD_WHITE}>> ${CLR_OK}Staging build... ${CLR_RST}"
# rsync -trph \
#     --info=progress2 \
#     --exclude '.*' \
#     "${APP_PATH}/" \
#     "${BUILD_SVR}:${BUILD_PATH}/"

# echo -e "${BOLD_WHITE}>> ${CLR_OK}Building... ${CLR_RST}"
# ssh $BUILD_SVR -t "cd ${BUILD_PATH} && make ${TARGET}"

# GAME_SVR_HOST=`yq e '.game_server.hostname' deploy.yaml`
# if [ -z "$GAME_SVR_HOST" ]; then
#     echo "${CLR_ERR}Error: missing game server hostname in deploy.yaml${CLR_RST}" >&2
#     exit 1
# fi

# GAME_SVR_PATH=`yq e '.game_server.path' deploy.yaml`
# if [ -z "$GAME_SVR_PATH" ]; then
#     echo "${CLR_ERR}Error: missing game server path in deploy.yaml${CLR_RST}" >&2
#     exit 1
# fi


# echo -e "${BOLD_WHITE}>> ${CLR_OK}All done... ${CLR_RST}"
