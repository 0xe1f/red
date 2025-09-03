#!/usr/bin/env python3

import itertools
import subprocess
import yaml

class GameConfig:

    def __init__(self, path):
        self.games = {}
        self.tags = set()
        self.genres = set()
        self.orientations = set()
        self.series = set()

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
            'uploads_supported': False,
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

class Config:

    def __init__(self, path):
        self.game_server = {}
        self.game_clients = []
        self.games = {}
        self.tags = set()
        self.genres = set()
        self.orientations = set()
        self.series = set()

        with open(path, 'r') as fd:
            conf = yaml.safe_load(fd)
            if 'game_server' in conf:
                self.game_server = GameServer(**conf['game_server'])
            if 'game_clients' in conf:
                for item in conf['game_clients']:
                    self.game_clients.append(GameClient(**item))
            if 'control_server' in conf:
                self.control_server = ControlServer(**conf['control_server'])

    def write_control_server_config(self, path):
        game_server = self.game_server.to_dict('host')
        game_clients = list(
            map(lambda client: client.to_dict('host'), self.game_clients)
        )

        with open(path, 'w') as fd:
            yaml.safe_dump(
                {
                    'game_server': game_server,
                    'game_clients': game_clients,
                },
                stream=fd,
            )

class Filter:

    def __init__(self, **item):
        self.__dict__.update(item)

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

    def to_dict(self, *remove):
        obj = self.__dict__
        for prop in remove:
            del obj[prop]
        return obj

    def launch(self):
        subprocess.call(
            args=[
                'ssh',
                self.client_ip,
                f'sudo killall {self.exe} 2> /dev/null;' + \
                    f'cd {self.path}; ' + \
                    f'sudo nohup ./{self.exe} {self.server_ip} {self.extra_args} >log.txt 2>&1 &'
            ],
        )

class GameServer:

    def __init__(self, **item):
        self.__dict__.update(item)

    def to_dict(self, *remove):
        obj = self.__dict__
        for prop in remove:
            del obj[prop]
        return obj

    def launch(self, game):
        # TODO: game.extra_args duplicates args. eliminate duping
        proc = subprocess.run(
            args=[
                'ssh',
                self.ip,
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
                self.ip,
                f'{self.path}/launch.sh --stop',
            ]
        )

class ControlServer:

    def __init__(self, **item):
        self.__dict__.update(item)

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
