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

# Change to the script's directory
cd "$(dirname "$0")"

echo "Activating virtual environment..." >&2
source .venv/bin/activate

echo "Starting server..." >&2
gunicorn \
    --access-logfile - --error-logfile - \
    --worker-class eventlet -b :8080 -w 1 launcher:app
