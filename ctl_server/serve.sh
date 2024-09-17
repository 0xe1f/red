#!/bin/bash

SERVER=serve.py

kill `ps -ef | awk '{ print $2" "$9 }' | grep $SERVER | cut -d ' ' -f 1` 2> /dev/null

python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

./$SERVER
