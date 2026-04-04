#!/usr/bin/env python3

# Copyright (C) 2024-2025 Akop Karapetyan
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import sys
import yaml

def write_remote_config(platform_config_path):
    with open(platform_config_path, 'r') as f:
        config = yaml.safe_load(f)

    output_doc = config['game_server']
    output_doc['nats_url'] = config['common']['nats_url']
    del output_doc['hostname']

    yaml.safe_dump(output_doc, sys.stdout)

    return config

def write_remote_games(game_config_path):
    with open(game_config_path, 'r') as f:
        config = yaml.safe_load(f)

    out_config = []
    for game in config:
        out_game = {
            'title_id': game['title_id'],
            'app_id': game['app_id'],
        }
        if 'extra_args' in game:
            out_game['extra_args'] = game['extra_args']
        out_config.append(out_game)

    yaml.safe_dump(out_config, sys.stdout)

def write_launcher_config(platform_config_path):
    with open(platform_config_path, 'r') as f:
        config = yaml.safe_load(f)

    out_config = {}
    out_config['sensor'] = config['sensor']
    out_config['game_server'] = {
        'nats_url': config['common']['nats_url'],
    }
    out_config['control_server'] = {
        'flask_config': config['control_server']['flask_config'],
    }

    yaml.safe_dump(out_config, sys.stdout)

def write_launcher_games(game_config_path):
    with open(game_config_path, 'r') as f:
        config = yaml.safe_load(f)

    out_config = []
    for game in config:
        out_game = game.copy()
        if 'extra_args' in out_game:
            del out_game['extra_args']
        out_config.append(out_game)

    yaml.safe_dump(out_config, sys.stdout)

def main():
    global konfig, game_konfig

    # Parse command-line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("--deploy-config", "-pc", help="Path to deployment configuration file (default: deploy.yaml)", default="deploy.yaml")
    parser.add_argument("--game-config", "-gc", help="Path to game configuration file (default: games.yaml)", default="games.yaml")
    parser.add_argument("--config", "-c", choices=["remote-config", "remote-games", "launcher-config", "launcher-games"], help="Type of config to write")
    args = parser.parse_args()

    if args.config == "remote-config":
        write_remote_config(args.deploy_config)
    elif args.config == "remote-games":
        write_remote_games(args.game_config)
    elif args.config == "launcher-config":
        write_launcher_config(args.deploy_config)
    elif args.config == "launcher-games":
        write_launcher_games(args.game_config)
    else:
        print("No config type specified, exiting.")

if __name__ == '__main__':
    main()
