#!/usr/bin/env python3

# LED Deploy script
# 2024 Akop Karapetyan

import argparse
import subprocess
import sys
import yaml

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
    ctl_server = yaml['ctl_server']

# if not game_server:
#     sys.exit('Missing game server configuration')

if args.ctl or args.all:
    if not ctl_server:
        sys.exit('Missing control server configuration')

    print("Deploying control server...")
    subprocess.call([
        "rsync",
        "-avr",
        "--delete",
        "ctl_server",
        f"{ctl_server}:"
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
