#!/usr/bin/env python3

import itertools
import subprocess
import sys
import yaml

class Config:

    def __init__(self, config_path, games_path):
        self.game_server = {}
        self.game_clients = []
        self.games = {}
        self.tags = set()
        self.genres = set()
        self.orientations = set()

        self.read_config(config_path)
        self.read_games(games_path)

    def read_config(self, path):
        with open(path) as fd:
            conf = yaml.safe_load(fd)
            self.game_server = GameServer(**conf['game_server'])
            for item in conf['game_clients']:
                self.game_clients.append(GameClient(**item))

        if not self.game_server:
            sys.exit('Missing game server configuration')
        if not self.game_clients:
            sys.exit('Missing game client configuration')

    def read_games(self, path):
        with open(path) as fd:
            for item in yaml.safe_load(fd):
                game = Game(**item)
                self.games[game.id] = game
                self.tags.update(game.tags)
                self.genres.update(game.genres)
                self.orientations.add(game.orientation)
    
    def template_args(self):
        return {
            'games': self.games.values(),
            'tags': self.tags,
            'genres': self.genres,
            'orientations': self.orientations,
        }

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

    def launch(self, game_server_host):
        subprocess.call([
            'ssh',
            self.host,
            f'sudo killall {self.exe} 2> /dev/null;' + \
                f'cd {self.path}; ' + \
                f'sudo nohup ./{self.exe} {game_server_host} {self.extra_args} >log.txt 2>&1 &'
        ])

class GameServer:

    def __init__(self, **item):
        self.__dict__.update(item)

    def launch(self, game):
        subprocess.call([
            'ssh',
            self.host,
            f'{self.path}/launch.sh {game.id} {self.extra_args}',
        ])

    def stop(self):
        subprocess.call([
            'ssh',
            self.host,
            f'{self.path}/launch.sh --stop',
        ])

class Game:

    def __init__(self, **item):
        self.__dict__.update(item)
        if 'tags' not in item:
            self.tags = []
        if 'genres' not in item:
            self.genres = []
        if 'orientation' not in item:
            self.orientation = 'landscape'
        self.filters = list(
            itertools.chain(
                map(lambda f: f"t:{f}", self.tags),
                map(lambda f: f"g:{f}", self.genres),
                [ f"o:{self.orientation}" ],
            )
        )
