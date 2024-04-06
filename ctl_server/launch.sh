#!/bin/bash

## Control Server Launcher
## 2024, Akop Karapetyan

CLIENT_EXE="rgbclient"
EMULATOR_EXE="fbneo"

TITLE=$1
shift

declare -A ARG_MAP

CLIENT_COUNT=-1
for arg in "$@"; do
    IFS='=' read -r NAME ARG <<< "$1"
    shift
    if [[ "$NAME" =~ ([0-9]+)$ ]]; then
        NUM=${BASH_REMATCH[1]}
        if [ "$NUM" -gt "$CLIENT_COUNT" ]; then
            CLIENT_COUNT=$NUM
        fi
    fi
    ARG_MAP[$NAME]=$ARG
done

CLIENT_COUNT=$((CLIENT_COUNT+1))

function runServer() {
    G_TITLE="$1"
    GS_HOST="$2"
    GS_PATH="$3"

    echo "Running game server on '${GS_HOST}'..."
    ssh \
        -o "StrictHostKeyChecking no" \
        "${GS_HOST}" \
        \
        "killall ${EMULATOR_EXE} 2> /dev/null; " \
        "cd ${GS_PATH}; " \
        "nohup ./${EMULATOR_EXE} " \
            "${G_TITLE} " \
            ">log.txt 2>&1 &"
}

function runClient() {
    GS_HOST="$1"
    C_HOST="$2"
    C_PATH="$3"
    C_SRECT="$4"
    C_DRECT="$5"
    C_ARGS="$6"

    echo "Running client on '${C_HOST}'..."
    ssh \
        -o "StrictHostKeyChecking no" \
        "${C_HOST}" \
        \
        "sudo killall ${CLIENT_EXE} " \
            "2> /dev/null; " \
        "cd ${C_PATH}; " \
        "sudo nohup ./${CLIENT_EXE} " \
            "${GS_HOST} " \
            "-srect ${C_SRECT} " \
            "-drect ${C_DRECT} " \
            "${C_ARGS} " \
            ">log.txt 2>&1 &"
}

# Wrap launch in a critical section
(
    flock -x -n 200 || echo "Failed to acquire lock" >&2 || exit 1

    runServer \
        "${TITLE}" \
        "${ARG_MAP[host]}" \
        "${ARG_MAP[path]}"

    for ((I=0;I<CLIENT_COUNT;++I)); do
        HOST_KEY=host$I
        PATH_KEY=path$I
        SRECT_KEY=srect$I
        DRECT_KEY=drect$I
        ARGS_KEY=args$I

        runClient \
            "${ARG_MAP[host]}" \
            "${ARG_MAP["$HOST_KEY"]}" \
            "${ARG_MAP["$PATH_KEY"]}" \
            "${ARG_MAP["$SRECT_KEY"]}" \
            "${ARG_MAP["$DRECT_KEY"]}" \
            "${ARG_MAP["$ARGS_KEY"]}"
    done
) 200>/var/lock/led-launch.lock
