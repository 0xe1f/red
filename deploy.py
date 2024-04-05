#!/usr/bin/env python3
import subprocess
import sys
import yaml

with open('deploy.yaml') as fd:
    yaml = yaml.safe_load(fd)
    game_server = yaml['game_server']
    game_clients = yaml['game_clients']
    server = yaml['server']

if not game_server:
    sys.exit('Missing game server configuration')
if not game_clients:
    sys.exit('Missing game client configuration')
if not server:
    sys.exit('Missing server configuration')

subprocess.call([
    "rsync",
    "-avr",
    "--delete",
    "ctl_server",
    f"{server}:"
])
