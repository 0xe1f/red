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

# Set up virtual environment and dependencies
if [ ! -d ".venv" ]; then
    echo "Creating virtual environment in .venv..." >&2
    python3 -m venv .venv
fi

echo "Activating virtual environment..." >&2
source .venv/bin/activate

echo "Installing dependencies..." >&2
pip install -r requirements.txt

echo "Starting server..." >&2
./remote.py
