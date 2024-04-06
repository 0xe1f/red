#!/bin/bash

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

echo "Running server..."

ssh \
    -o "StrictHostKeyChecking no" \
    "${ARG_MAP[host]}" \
    "killall fbneo 2> /dev/null; cd ${ARG_MAP[path]}; nohup ./fbneo ${TITLE} >/dev/null 2>&1 &"

echo "Running clients ($CLIENT_COUNT)..."

for ((I=0;I<CLIENT_COUNT;++I)); do
    HOST_KEY=host$I
    PATH_KEY=path$I
    SRECT_KEY=srect$I
    DRECT_KEY=drect$I
    ARGS_KEY=args$I

    ssh \
        -o "StrictHostKeyChecking no" \
        "${ARG_MAP["$HOST_KEY"]}" \
        "sudo killall rgbclient 2> /dev/null; cd ${ARG_MAP["$PATH_KEY"]}; sleep 2; sudo nohup ./rgbclient ${ARG_MAP[host]} -srect ${ARG_MAP["$SRECT_KEY"]} -drect ${ARG_MAP["$DRECT_KEY"]} ${ARG_MAP["$ARGS_KEY"]} >log.txt 2>&1 &"

done
