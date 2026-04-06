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

NATS_URL=`yq e '.common.nats_url' deploy.yaml`
if [ -z "${NATS_URL}" ]; then
    echo "${CLR_ERR}Error: missing NATS URL in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

BUILD_SVR=`yq e '.common.build_host' deploy.yaml`
if [ -z "${BUILD_SVR}" ]; then
    echo "${CLR_ERR}Error: missing build server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

CLIENT_COUNT=`yq e ".subscribers | length - 1" deploy.yaml`
if [ $CLIENT_COUNT -le 0 ] ; then
    echo "${CLR_ERR}Error: missing subscriber config in deploy.yaml${CLR_RST}" >&2
    exit 1
fi

LOCAL_APP_PATH=pubsub
BUILD_PATH="red_builds/${LOCAL_APP_PATH}"
TARGET=sub
SUB_SVC_FILE=sub.service

echo -e "${BOLD_WHITE}>> ${CLR_OK}Staging build... ${CLR_RST}"
ssh "${BUILD_SVR}" "mkdir -p \"${BUILD_PATH}\""
rsync -trph \
    --info=progress2 \
    --exclude '.*' \
    "${LOCAL_APP_PATH}/" \
    "${BUILD_SVR}:${BUILD_PATH}"

echo -e "${BOLD_WHITE}>> ${CLR_OK}Building... ${CLR_RST}"
ssh $BUILD_SVR "cd ${BUILD_PATH} && make ${TARGET}"

TEMP_DIR=`mktemp -d`
function cleanup {
  echo "Removing ${TEMP_DIR}"
  rm  -r "${TEMP_DIR}"
}
trap cleanup EXIT

# Copy the generated executable locally
rsync -trph \
    "${BUILD_SVR}:${BUILD_PATH}/${TARGET}" \
    "${TEMP_DIR}/"

complete_setup() {
    local host=$1
    local service_path=$2
    local svc_file=$3
    local executable=$4
    local args=$5

    ssh "${host}" "
        (
            SYSTEMD_PATH=\"\${HOME}/.config/systemd/user\" &&
            SVC_PATH=\"\${SYSTEMD_PATH}/red_${svc_file}\" &&
            test -f \"\${SVC_PATH}\" 2>/dev/null ||
            (
                echo \"Setting up systemd service...\" >&2 &&
                mkdir -p \"\${SYSTEMD_PATH}\" &&
                P=\$(readlink -f \"${service_path}/${executable}\") &&
                cat \"${service_path}/${svc_file}\" | sed \
                    -e \"s|{SERVICE_PATH}|\${P}|g\" \
                    -e \"s|{ARGS}|${args}|g\" \
                    -e \"s|{NATS_URL}|${NATS_URL}|g\" > \"\${SVC_PATH}\" &&
                systemctl --user daemon-reload &&
                systemctl --user enable \"red_${svc_file}\" &&
                sudo loginctl enable-linger \"\${USER}\"
            )
        ) &&
        (
            echo \"Restarting service...\" >&2 &&
            systemctl --user restart \"red_${svc_file}\" &&
            rm \"${service_path}/${svc_file}\"
        )
    "
}

# Deploy to clients
for CLIENT in `seq 0 $CLIENT_COUNT`; do
    CL_HOST=`yq e ".subscribers[$CLIENT].hostname" deploy.yaml`
    CL_PATH=`yq e ".subscribers[$CLIENT].path" deploy.yaml`
    CL_ARGS=`yq e ".subscribers[$CLIENT].args" deploy.yaml`

    echo -e "${BOLD_WHITE}>> ${CLR_OK}Deploying to ${CL_HOST}... ${CLR_RST}"
    ssh "${CL_HOST}" "mkdir -p ${CL_PATH}"
    rsync -trph \
        "${TEMP_DIR}/${TARGET}" \
        "${LOCAL_APP_PATH}/${SUB_SVC_FILE}" \
        "${CL_HOST}:${CL_PATH}"

    complete_setup "${CL_HOST}" "${CL_PATH}" "${SUB_SVC_FILE}" "${TARGET}" "${CL_ARGS}"
done

echo -e "${BOLD_WHITE}>> ${CLR_OK}All done... ${CLR_RST}"
