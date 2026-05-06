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

try_libretro_build() {
    local makefile="$1"
    local mf_dir

    if [ -n "$makefile" ]; then
        echo "Using Makefile: $makefile"
        mf_dir=$(dirname "$makefile")
        pushd "$mf_dir" > /dev/null && \
            make -f "$(basename "$makefile")" && \
            popd > /dev/null && \
            echo "done:$(find "$mf_dir" -maxdepth 1 -type f -name '*_libretro.so')" && \
            exit 0
        exit 1
    fi
}

try_custom_build() {
    if [ -f "../${MODULE_NAME}.build" ]; then
        echo "Running custom build script for $MODULE_NAME"
        source "../${MODULE_NAME}.build" || exit $?
        exit 0
    fi
}

MODULE_NAME=$1
if [ -z "$MODULE_NAME" ]; then
    echo "ERROR: Run bcores.sh from root" >&2
    # echo "Error: module_name is missing" >&2
    exit 1
fi

cd $MODULE_NAME

# Try a custom build script first, if it exists
try_custom_build
# See if there's a Makefile.libretro and build that
try_libretro_build `find . -type f -name "Makefile.libretro"`
# See if there's a Makefile in a libretro subdirectory and build that
try_libretro_build `find . -path '*/libretro/*' -name "Makefile" -type f | head -1`
# Finally, see if there's a Makefile anywhere and build that
try_libretro_build `find . -name "Makefile" -type f | head -1`

echo "Unrecognized or unbuildable core: $MODULE_NAME" >&2
exit 1
