#!/bin/bash

SERVER=serve.py

kill `ps -ef | awk '{ print $2" "$9 }' | grep $SERVER | cut -d ' ' -f 1` 2> /dev/null
./$SERVER
