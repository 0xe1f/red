#!/usr/bin/env python3

# LED Deploy script
# 2024 Akop Karapetyan

import argparse
import subprocess
import sys
import yaml

CTL_SERVER_EXE='serve.py'
CTL_SERVER_PATH='ctl_server'

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

parser = argparse.ArgumentParser()
parser.add_argument('--ctl', action='store_true', help='Deploy to control server')
parser.add_argument('--cli', action='store_true', help='Deploy to clients')
parser.add_argument('--all', action='store_true', help='Deploy everything')
args = parser.parse_args()

if not (args.ctl or args.cli or args.all):
    sys.exit('Specify at least one option or "--all" to deploy everything')

with open('deploy.yaml') as fd:
    yaml = yaml.safe_load(fd)
    # game_server = yaml['game_server']
    game_clients = [GameClient(**x) for x in yaml['game_clients']]
    ctl_server_host = yaml['ctl_server']

# if not game_server:
#     sys.exit('Missing game server configuration')

if args.ctl or args.all:
    if not ctl_server_host:
        sys.exit('Missing control server configuration')

    print("Deploying control server...")
    subprocess.call([
        'rsync',
        '-avr',
        '--delete',
        '--exclude', '*_example.yaml',
        f'{CTL_SERVER_PATH}',
        f'{ctl_server_host}:'
    ])
    subprocess.call([
        'ssh',
        '-o',
        'StrictHostKeyChecking no',
        f'{ctl_server_host}',
        f"kill `ps -ef | grep python | grep {CTL_SERVER_EXE} | sort -k2 -n | head -1 | awk '{{print $2}}'` 2> /dev/null; " \
        f'cd {CTL_SERVER_PATH}; nohup ./{CTL_SERVER_EXE} >log.txt 2>&1 &',
    ])

if args.cli or args.all:
    if not game_clients:
        sys.exit('Missing client configuration')

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
