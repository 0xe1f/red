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

# Stages, builds and deploys game servers to the game server host

# Check prerequisites
if [ ! -f "deploy.yaml" ]; then
    echo "Error: deploy.yaml not found" >&2
    exit 1
elif ! command -v yq &> /dev/null; then
    echo "Error: yq is not installed" >&2
    exit 1
fi

BUILD_SVR=`yq e '.control_server.hostname' deploy.yaml`
if [ -z "$BUILD_SVR" ]; then
    echo "Error: missing build server hostname in deploy.yaml" >&2
    exit 1
fi

GAME_SVR_HOST=`yq e '.game_server.hostname' deploy.yaml`
if [ -z "$GAME_SVR_HOST" ]; then
    echo "Error: missing game server hostname in deploy.yaml" >&2
    exit 1
fi

GAME_SVR_PATH=`yq e '.game_server.path' deploy.yaml`
if [ -z "$GAME_SVR_PATH" ]; then
    echo "Error: missing game server path in deploy.yaml" >&2
    exit 1
fi

ARGS="$@"
if [ "$1" == "--all" ] || [ "$1" == "-a" ]; then
    # List all game servers in the game_servers directory,
    # excluding hidden and dependency directories
    ARGS=$(find game_servers/ -mindepth 1 -maxdepth 1 -type d -not -name '.*' -not -name deps -exec basename {} \; | tr -s '[:space:]' ' ')
    shift

    if [ $# -gt 0 ]; then
        echo "Error: unexpected arguments after --all" >&2
        exit 1
    fi
fi

# Run the staging script
game_servers/stage.sh "$BUILD_SVR" "$GAME_SVR_HOST" "$GAME_SVR_PATH" $ARGS
