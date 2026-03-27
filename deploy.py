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
parser.add_argument('--all', action='store_true', help='Deploy everything')
args = parser.parse_args()

if not (args.ctl or args.all):
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
