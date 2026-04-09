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

NETWORK=${NETWORK:-"red"}
NAME="launcher.$NETWORK"
IMAGE="$NETWORK/launcher"
HTTP_PORT=${HTTP_PORT:-8080}
HTTPS_PORT=${HTTPS_PORT:-8443}

docker run --name $NAME -d \
    --publish 8080:$HTTP_PORT \
    --publish 8443:$HTTPS_PORT \
    --network $NETWORK \
    --rm \
    $IMAGE \
    $@
