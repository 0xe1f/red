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

# Check if protoc is installed before erroring out
if ! command -v protoc &> /dev/null; then
    echo "Error: protoc is not installed. Build separately or install it" >&2
    exit 1
fi

GENERATED_DIR="generated"

# Compile protobuf definitions
echo "Compiling protobuf definitions..." >&2
mkdir -p "$GENERATED_DIR"
protoc -I=proto --python_out="$GENERATED_DIR" proto/*.proto

# Fix imports in generated files
TMPFILE=$(mktemp)
for file in "$GENERATED_DIR"/*; do
    cat "$file" | sed "s/^import common_pb2/import $GENERATED_DIR.common_pb2/" > "$TMPFILE"
    mv "$TMPFILE" "$file"
    # Set the same permissions as the original file
    chmod --reference="$GENERATED_DIR/common_pb2.py" "$file"
done

# Done
echo "Build complete." >&2
