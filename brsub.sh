#!/bin/bash

cd "$(dirname "$0")"

set -e

BUILD_SVR=`yq e '.common.build_host' deploy.yaml`
if [ -z "${BUILD_SVR}" ]; then
    echo "${CLR_ERR}Error: missing build server hostname in deploy.yaml${CLR_RST}" >&2
    exit 1
fi
LOCAL_APP_PATH=rsub
BUILD_PATH=red_builds

rsync -tprh ${LOCAL_APP_PATH} --exclude 'target/' ${BUILD_SVR}:${BUILD_PATH}
ssh "${BUILD_SVR}" "source \$HOME/.cargo/env && cd ${BUILD_PATH}/${LOCAL_APP_PATH} && cargo build --release"
