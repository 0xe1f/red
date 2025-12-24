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

from collections import defaultdict
import itertools
import subprocess
import yaml

class GameConfig:

    def __init__(self, path):
        self.game_map = {}
        self.tags = defaultdict(list)
        self.genres = defaultdict(list)
        self.orientations = defaultdict(list)
        self.series = defaultdict(list)
        self.platforms = defaultdict(list)

        with open(path) as fd:
            for item in yaml.safe_load(fd):
                game = Game(**item)
                self.game_map[game.id] = game
                for o in game.tags:
                    self.tags[o].append(game.id)
                for o in game.genres:
                    self.genres[o].append(game.id)
                self.orientations[game.orientation].append(game.id)
                for o in game.series:
                    self.series[o].append(game.id)
                self.platforms[game.app_id].append(game.id)

    def filters(self):
        return [
            {
                "id": 'orientation',
                "label": 'Orientation',
                "options": sorted([
                    {
                        "name": k,
                        "count": len(v),
                    } for k, v in self.orientations.items()], key=lambda x: x['name'].lower()),
                "prefix": 'o',
            },
            {
                "id": 'platform',
                "label": 'Platform',
                "options": sorted([
                    {
                        "name": k,
                        "count": len(v),
                    } for k, v in self.platforms.items()], key=lambda x: x['name'].lower()),
                "prefix": 'p',
            },
            {
                "id": 'genre',
                "label": 'Genres',
                "options": sorted([
                    {
                        "name": k,
                        "count": len(v),
                    } for k, v in self.genres.items()], key=lambda x: x['name'].lower()),
                "prefix": 'g',
            },
            {
                "id": 'tag',
                "label": 'Tags',
                "options": sorted([
                    {
                        "name": k,
                        "count": len(v),
                    } for k, v in self.tags.items()], key=lambda x: x['name'].lower()),
                "prefix": 't',
                "type": 'multi',
            },
            {
                "id": 'series',
                "label": 'Series',
                "options": sorted([
                    {
                        "name": k,
                        "count": len(v),
                    } for k, v in self.series.items()], key=lambda x: x['name'].lower()),
                "prefix": 's',
            },
        ]

    def games(self, search="", filter_map={}):
        all_games = self.game_map.values()
        if not (search or filter_map):
            return sorted([ game.as_dict() for game in all_games ], key=lambda x: x['title'].lower())

        filtered_games = []
        for game in all_games:
            if search and search.lower() not in game.title.lower():
                continue
            filter_match = True
            for (k, v_list) in filter_map.items():
                filters = set(v_list)
                if k == 't' and filters.intersection(set(game.tags)) != filters:
                    filter_match = False
                    break
                elif k == 'o' and filters != { game.orientation }:
                    filter_match = False
                    break
                elif k == 'p' and filters != { game.app_id }:
                    filter_match = False
                    break
                elif k == 'g' and filters.intersection(set(game.genres)) != filters:
                    filter_match = False
                    break
                elif k == 's' and filters.intersection(set(game.series)) != filters:
                    filter_match = False
                    break
            if filter_match:
                filtered_games.append(game)

        return sorted([ game.as_dict() for game in filtered_games ], key=lambda x: x['title'].lower())

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
        extra_args = []
        if hasattr(self, 'platforms'):
            extra_args.append(
                self.platforms.get('common', {}).get('extra_args', '')
            )
            extra_args.append(
                self.platforms.get(game.app_id, {}).get('extra_args', '')
            )
        extra_args.append(game.extra_args)

        proc = subprocess.run(
            args=[
                'ssh',
                self.ip,
                f"{self.path}/launch.sh {game.app_id} {game.title_id} {' '.join(filter(None, extra_args))}",
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
                f'{self.path}/stopall.sh',
            ]
        )

class ControlServer:

    def __init__(self, **item):
        self.__dict__.update(item)

class Game:

    def __init__(self, **item):
        self.__dict__.update(item)

        if 'app_id' not in item:
            self.app_id = 'fbneo'
        self.id = f'{self.app_id}:{self.title_id}'
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

    def as_dict(self):
        return { k: v for k, v in self.__dict__.items() if v }
