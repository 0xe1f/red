#!/usr/bin/env python3

## Control Server
## 2024, Akop Karapetyan

import glob
import http.server
import http
import itertools
import jinja2
import json
import logging
import mimetypes
import os
import subprocess
import sys
import urllib.parse
import yaml

TEMPLATE_PATH = 'templates'
game_server = ""
game_clients = []
templates = {}
games = {}
tags = set()
genres = set()
orientations = set()

class Handler(http.server.BaseHTTPRequestHandler):

    def write_response_headers(self, code = 200, type = 'text/html'):
        self.send_response(code)
        self.send_header('Content-type', type)
        self.end_headers()

    def do_GET(self):
        logging.info("GET %s", str(self.path))
        path = os.path.normpath(self.path).lstrip('/')
        if path.startswith('static/'):
            self.write_file(path)
        elif path == 'query':
            result = self.read_process_output(
                'ssh',
                [
                    game_server.host,
                    f'{game_server.path}/query.sh',
                ]
            )
            title, vol, *_ = result.split(' ', 1) + [None] * 2
            self.write_json({
                'title': title,
                'volume': vol,
            })
        else:
            template = templates.get(path if path else 'index')
            self.write_template(
                template,
                games = games.values(),
                tags = tags,
                genres = genres,
                orientations = orientations,
            )

    def do_POST(self):
        logging.info("POST %s", str(self.path))
        path = urllib.parse.urlparse(self.path).path.lstrip('/')
        if path == 'launch':
            payload = json.loads(self.read_post())
            self.launch(**payload)
        elif path == 'volume':
            payload = json.loads(self.read_post())
            self.set_volume(**payload)
        else:
            self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")

    def set_volume(self, **kwargs):
        volume = kwargs.get('volume')
        if not 0 <= volume <= 100:
            self.send_error(http.HTTPStatus.FORBIDDEN, "Value not valid")
        subprocess.call([
            'ssh',
            game_server.host,
            f'{game_server.path}/set_volume.sh {volume}',
        ])
        self.write_json({ 'status': 'OK'  })

    def launch(self, **kwargs):
        id = kwargs.get('id')
        if not id:
            logging.error('Missing id')
            self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")
            return
        game = games[id]
        if not id:
            logging.error('Game not found')
            self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")
            return

        game_server.launch(game)
        for client in game_clients:
            client.launch(game_server.host)

        self.write_json({ 'status': 'OK'  })

    def read_post(self):
        content_length = int(self.headers['Content-Length'])
        return self.rfile.read(content_length).decode('utf-8')

    def write_template(self, template, **kwargs):
        logging.info("write_template()")
        if not template:
            self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")
            return

        self.write_response_headers()
        self.wfile.write(template.render(kwargs).encode('utf-8'))

    def write_text(self, content):
        logging.info("write_text()")
        self.write_response_headers()
        self.wfile.write(content.encode('utf-8'))

    def write_file(self, path):
        logging.info("write_file: %s", path)
        if not os.path.isfile(path):
            self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")
            return

        mt = mimetypes.guess_type(path)
        self.write_response_headers(type = mt[0])
        with open(path, 'rb') as file:
            self.wfile.write(file.read())

    def write_json(self, obj):
        logging.info("write_json: %s", obj)
        self.write_response_headers(type = 'application/json')
        self.wfile.write(json.dumps(obj).encode('utf-8'))

    def read_process_output(self, exe, args):
        result = subprocess.run([exe] + args, stdout=subprocess.PIPE)
        return result.stdout.decode('utf-8').rstrip()

def run(server_class = http.server.ThreadingHTTPServer, handler_class = Handler, port = 8080):
    logging.basicConfig(level = logging.DEBUG)
    server_address = ('0.0.0.0', port)
    httpd = server_class(server_address, handler_class)
    logging.info('Starting httpd...\n')
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    logging.info('Stopping httpd...\n')

class Game:

    def __init__(self, **item):
        self.__dict__.update(item)
        if 'tags' not in item:
            self.tags = []
        if 'genres' not in item:
            self.genres = []
        if 'orientation' not in item:
            self.orientation = 'landscape'
        self.filters = itertools.chain(
            map(lambda f: f"t:{f}", self.tags),
            map(lambda f: f"g:{f}", self.genres),
            [ f"o:{self.orientation}" ],
        )

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

    def launch(self, game_server_host):
        logging.debug(f'Launching client on {self.host}')
        subprocess.call([
            'ssh',
            self.host,
            f'sudo killall {self.exe} 2> /dev/null;' + \
                f'cd {self.path}; ' + \
                f'sudo nohup ./{self.exe} {game_server_host} --src-rect={self.srect} --dest-rect={self.drect} {self.extra_args} >log.txt 2>&1 &'
        ])

class GameServer:

    def __init__(self, **item):
        self.__dict__.update(item)

    def launch(self, game):
        logging.debug(f'Launching with {game.id} on {self.host}')
        subprocess.call([
            'ssh',
            self.host,
            f'{self.path}/launch.sh {game.id} {self.extra_args}',
        ])

def init_templates():
    global templates
    environment = jinja2.Environment(loader = jinja2.FileSystemLoader(TEMPLATE_PATH))
    for file in glob.glob("{}/*.jinja".format(TEMPLATE_PATH)):
        base = os.path.basename(file)
        name = os.path.splitext(base)[0]
        templates[name] = environment.get_template(base)

def init_games():
    global games
    with open("games.yaml") as fd:
        for item in yaml.safe_load(fd):
            game = Game(**item)
            games[game.id] = game
            tags.update(game.tags)
            genres.update(game.genres)
            orientations.add(game.orientation)

def init_config():
    global game_server
    global game_clients
    with open('config.yaml') as fd:
        conf = yaml.safe_load(fd)
        game_server = GameServer(**conf['game_server'])
        for item in conf['game_clients']:
            game_clients.append(GameClient(**item))

    if not game_server:
        sys.exit('Missing game server configuration')
    if not game_clients:
        sys.exit('Missing game client configuration')

init_config()
init_templates()
init_games()

if __name__ == '__main__':
    from sys import argv

    if len(argv) == 2:
        run(port = int(argv[1]))
    else:
        run()
