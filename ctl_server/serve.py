#!/usr/bin/env python3
import glob
import http.server
import http
import jinja2
import json
import logging
import mimetypes
import os
import re
import subprocess
import sys
import urllib.parse
import yaml

template_path = 'templates'
game_server = ""
game_clients = []
templates = {}
games = {}

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
        else:
            template = templates.get(path if path else 'index')
            self.write_template(template, games = games.values())

    def do_POST(self):
        logging.info("POST %s", str(self.path))
        path = urllib.parse.urlparse(self.path).path.lstrip('/')
        if path == 'launch':
            payload = json.loads(self.read_post())
            self.launch(**payload)
        else:
            self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")

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

        launch(game)
        # FIXME
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

def run(server_class = http.server.HTTPServer, handler_class = Handler, port = 8080):
    logging.basicConfig(level = logging.INFO)
    server_address = ('', port)
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

class Rect:

    def __init__(self, str):
        m = re.match(r'^([0-9]+),([0-9]+)-([0-9]+),([0-9]+)$', str)
        if not m:
            raise Exception(f"{str} is not a valid rect")

        self.x = int(m.group(1))
        self.y = int(m.group(2))
        self.w = int(m.group(3))
        self.h = int(m.group(4))

class GameClient:

    def __init__(self, item):
        self.host = item['host']
        self.path = item['path']
        self.srect = item['srect']
        self.drect = item['drect']

    def launch_cmd(self, game_host):
        return f'killall rgbclient 2> /dev/null; cd {self.path}; sleep 2; sudo nohup ./lcli.sh {game_host} -srect {self.srect} -drect {self.drect} >/dev/null 2>&1 &'

class GameServer:

    def __init__(self, **item):
        self.__dict__.update(item)

    def launch_cmd(self, game_cmd):
        return f'killall fbneo 2> /dev/null; cd {self.path}; nohup ./{game_cmd} >/dev/null 2>&1 &'

def init_templates():
    global templates
    environment = jinja2.Environment(loader = jinja2.FileSystemLoader(template_path))
    for file in glob.glob("{}/*.jinja".format(template_path)):
        base = os.path.basename(file)
        name = os.path.splitext(base)[0]
        templates[name] = environment.get_template(base)

def init_games():
    global games
    with open("games.yaml") as fd:
        for item in yaml.safe_load(fd):
            game = Game(**item)
            games[game.id] = game

def init_config():
    global game_server
    global game_clients
    with open('conf.yaml') as fd:
        conf = yaml.safe_load(fd)
        game_server = GameServer(**conf['game_server'])
        for item in conf['game_clients']:
            game_clients.append(GameClient(item))

    if not game_server:
        sys.exit('Missing game server configuration')
    if not game_clients:
        sys.exit('Missing game client configuration')

def launch(game):
    logging.info(f'Launching server ({game.id})...')
    subprocess.call([
        'ssh',
        '-o',
        'StrictHostKeyChecking no',
        game_server.host, # TODO: user too
        game_server.launch_cmd(game.server_cmd),
    ])
    logging.info(f'Launching clients ({len(game_clients)})...')
    for client in game_clients:
        subprocess.call([
            'ssh',
            '-o',
            'StrictHostKeyChecking no',
            client.host, # TODO: user too
            client.launch_cmd(game_server.host),
        ])
    logging.info(f'done!')

init_config()
init_templates()
init_games()

if __name__ == '__main__':
    from sys import argv

    if len(argv) == 2:
        run(port = int(argv[1]))
    else:
        run()
