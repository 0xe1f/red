#!/usr/bin/env python3
import subprocess
import sys
import yaml

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

with open('deploy.yaml') as fd:
    yaml = yaml.safe_load(fd)
    game_server = yaml['game_server']
    game_clients = [GameClient(**x) for x in yaml['game_clients']]
    server = yaml['server']

if not game_server:
    sys.exit('Missing game server configuration')
if not game_clients:
    sys.exit('Missing game client configuration')
if not server:
    sys.exit('Missing server configuration')

print("Deploying menu...")
subprocess.call([
    "rsync",
    "-avr",
    "--delete",
    "ctl_server",
    f"{server}:"
])

print("Deploying to clients...")
for client in game_clients:
    # Copy rgbclient
    subprocess.call([
        'rsync',
        '-avr',
        '--exclude', '.*',
        f'rgbclient/',
        f'{client.host}:{client.path}',
    ])
    # Build
    subprocess.call([
        'ssh',
        '-o',
        'StrictHostKeyChecking no',
        f'{client.host}',
        '-t',
        f'cd {client.path} && make',
    ])
