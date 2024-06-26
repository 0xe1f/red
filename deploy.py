#!/usr/bin/env python3

# LED Deploy script
# 2024 Akop Karapetyan

import argparse
import subprocess
import sys
import yaml

CTL_SERVER_EXE='serve.sh'

class GameServer:

    def __init__(self, **item):
        self.__dict__.update(item)

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

class ControlServer:

    def __init__(self, **item):
        self.__dict__.update(item)

parser = argparse.ArgumentParser()
parser.add_argument('--ctl', action='store_true', help='Deploy to control server')
parser.add_argument('--cli', action='store_true', help='Deploy to clients')
parser.add_argument('--svr', action='store_true', help='Deploy to game server')
parser.add_argument('--all', action='store_true', help='Deploy everything')
args = parser.parse_args()

if not (args.ctl or args.svr or args.cli or args.all):
    sys.exit('Specify at least one option or "--all" to deploy everything')

with open('deploy.yaml') as fd:
    yaml = yaml.safe_load(fd)
    game_server = GameServer(**yaml['game_server'])
    game_clients = [GameClient(**x) for x in yaml['game_clients']]
    ctl_server = ControlServer(**yaml['control_server'])

if args.ctl or args.all:
    if not ctl_server:
        sys.exit('Missing control server configuration')

    print("Deploying control server...")
    subprocess.call([
        'rsync',
        '-avrL',
        '--delete',
        '--exclude', '.*',
        '--exclude', '*_example.yaml',
        '--exclude', '__pycache__',
        f'{ctl_server.path}',
        f'{ctl_server.host}:'
    ])
    subprocess.call([
        'ssh',
        '-o',
        'StrictHostKeyChecking no',
        f'{ctl_server.host}',
        f'cd {ctl_server.path}; nohup ./{CTL_SERVER_EXE} >log.txt 2>&1 &',
    ])

if args.svr or args.all:
    if not game_server:
        sys.exit('Missing game server configuration')

    print("Deploying to server...")
    subprocess.call([
        'rsync',
        '-vrtp',
        '--exclude', '*.exclude',
        '--exclude-from', 'game_servers/common.exclude',
        '--exclude-from', 'game_servers/fbneo.exclude',
        f'game_servers/',
        f'{game_server.host}:{game_server.path}',
    ])

    print("Building server(s)...")
    subprocess.call([
        'ssh',
        '-o',
        'StrictHostKeyChecking no',
        f'{game_server.host}',
        '-t',
        f'cd {game_server.path} && ./build.sh',
    ])

if args.cli or args.all:
    if not game_clients:
        sys.exit('Missing client configuration')

    print("Deploying to clients...")
    for client in game_clients:
        # Copy rgbclient
        subprocess.call([
            'rsync',
            '-vrt',
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
