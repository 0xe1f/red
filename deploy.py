#!/usr/bin/env python3

# LED Deploy script
# 2024 Akop Karapetyan

import argparse
from ctl_server import config
import subprocess
import sys

CTL_SERVER_EXE='serve.sh'

parser = argparse.ArgumentParser()
parser.add_argument('--ctl', action='store_true', help='Deploy to control server')
parser.add_argument('--cli', action='store_true', help='Deploy to clients')
parser.add_argument('--svr', nargs='*', help='Deploy to game server')
parser.add_argument('--all', action='store_true', help='Deploy everything')
args = parser.parse_args()

if not (args.ctl or args.svr is not None or args.cli or args.all):
    sys.exit('Specify at least one option or "--all" to deploy everything')

config = config.Config('deploy.yaml')

if args.ctl or args.all:
    if not config.control_server:
        raise ValueError('Missing control server configuration')

    print("Generating config...")
    config.write_control_server_config(f'{config.control_server.path}/config.yaml')

    print("Deploying control server...")
    subprocess.call([
        'rsync',
        '-avrL',
        '--exclude', '.*',
        '--exclude', '*.db',
        '--exclude', '*.example',
        '--exclude', '__pycache__',
        '--exclude', 'venv/',
        f'{config.control_server.path}',
        f'{config.control_server.hostname}:'
    ])
    subprocess.call([
        'ssh',
        '-o',
        'StrictHostKeyChecking no',
        f'{config.control_server.hostname}',
        f'cd {config.control_server.path}; nohup ./{CTL_SERVER_EXE} >log.txt 2>&1 &',
    ])

if args.svr is not None or args.all:
    if not config.game_server:
        raise ValueError('Missing game server configuration')

    subprocess.call([
        'game_servers/stage.sh',
        config.control_server.hostname,
        config.game_server.hostname,
        config.game_server.path,
        *(args.svr or []),
    ])

if args.cli or args.all:
    if not config.game_clients:
        raise ValueError('Missing game client configuration')

    print("Deploying to clients...")
    for client in config.game_clients:
        # Copy rgbclient
        subprocess.call([
            'rsync',
            '-vrt',
            '--exclude', '.*',
            f'clients/rgbclient/',
            f'{client.hostname}:{client.path}',
        ])
        # Build
        subprocess.call([
            'ssh',
            '-o',
            'StrictHostKeyChecking no',
            f'{client.hostname}',
            '-t',
            f'cd {client.path} && make',
        ])
