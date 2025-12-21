#!/bin/bash

HOST="$1"
SVR_PATH="$2"

if [ -z "$HOST" ] || [ -z "$SVR_PATH" ]; then
    echo "Error: missing arguments. Usage: $0 host game_server_path" >&2
    exit 1
fi

cd `dirname $(readlink -f $0)`
shift 2

# If no projects are specified, build all directories
ARGS="$@"
if [ -z "$ARGS" ]; then
    ARGS=$(find . -maxdepth 1 -type d -not -name '.*')
fi

# Check that all specified directories have a build.sh
for DIR in $ARGS; do
    if [ ! -f "$DIR/build.sh" ]; then
        echo "Error: No build.sh found in $DIR" >&2
        exit 1
    fi
done

set -e
rsync -vt --exclude='*/' --exclude='build.sh' * $HOST:$SVR_PATH

# Run build.sh in each specified directory
for DIR in $ARGS; do
    DIRNAME=$(basename $DIR)
    echo "Building $DIRNAME..."

    pushd $DIRNAME > /dev/null

    # Create remote directory
    ssh -o "StrictHostKeyChecking no" $HOST -t "mkdir -p $SVR_PATH/$DIRNAME"
    # Run build script and capture output
    BUILD_OUTPUT=$(./build.sh | tee /dev/tty)
    # Get last line of output as files to copy
    FILES=$(echo "$BUILD_OUTPUT" | tail -n 1)
    # Copy built files to remote server
    rsync -vtrp $FILES launch.sh stop.sh "$HOST:$SVR_PATH/$DIRNAME/"

    popd > /dev/null
done
