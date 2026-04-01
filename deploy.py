#!/usr/bin/env python3

# LED Deploy script
# 2024 Akop Karapetyan

from ctl_server import config
import subprocess

CTL_SERVER_EXE='serve.sh'

config = config.Config('deploy.yaml')

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

# FIXME: deploy config files too
# FIXME: strip non-essential data from platform config
# FIXME: strip non-essential data from games config
