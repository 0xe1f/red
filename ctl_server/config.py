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
        self.series = set()

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
                self.series.update(game.series)

    def template_args(self):
        return {
            'games': self.games.values(),
            'uploads_supported': hasattr(self.game_server, 'rom_path'),
            'filters': [
                Filter(
                    id='orientation',
                    label='Orientation',
                    options=self.orientations,
                    prefix='o',
                ),
                Filter(
                    id='genre',
                    label='Genres',
                    options=self.genres,
                    prefix='g',
                ),
                Filter(
                    id='tag',
                    label='Tags',
                    options=self.tags,
                    prefix='t',
                    type='multi',
                ),
                Filter(
                    id='series',
                    label='Series',
                    options=self.series,
                    prefix='s',
                ),
            ],
        }

class Filter:
    def __init__(self, **item):
        self.__dict__.update(item)

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

    def launch(self, game_server_host):
        subprocess.call(
            args=[
                'ssh',
                self.host,
                f'sudo killall {self.exe} 2> /dev/null;' + \
                    f'cd {self.path}; ' + \
                    f'sudo nohup ./{self.exe} {game_server_host} {self.extra_args} >log.txt 2>&1 &'
            ],
        )

class GameServer:

    def __init__(self, **item):
        self.__dict__.update(item)

    def launch(self, game):
        # TODO: game.extra_args duplicates args. eliminate duping
        proc = subprocess.run(
            args=[
                'ssh',
                self.host,
                f'{self.path}/launch.sh {game.id} {self.extra_args} {game.extra_args}',
            ],
            capture_output=True,
            text=True,
        )

        return proc.returncode, proc.stdout

    def stop(self):
        subprocess.call(
            args=[
                'ssh',
                self.host,
                f'{self.path}/launch.sh --stop',
            ]
        )

class Game:

    def __init__(self, **item):
        self.__dict__.update(item)
        if 'tags' not in item:
            self.tags = []
        if 'genres' not in item:
            self.genres = []
        if 'orientation' not in item:
            self.orientation = 'landscape'
        if 'series' not in item:
            self.series = []
        if 'extra_args' not in item:
            self.extra_args = ""
        self.filters = list(
            itertools.chain(
                map(lambda f: f"t:{f}", self.tags),
                map(lambda f: f"g:{f}", self.genres),
                map(lambda f: f"s:{f}", self.series),
                [ f"o:{self.orientation}" ],
            )
        )
