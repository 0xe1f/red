#!/usr/bin/env python3
import glob
import http.server
import http
import jinja2
import json
import logging
import mimetypes
import os
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

        subprocess.call(
            [
                './launch.sh',
                game.id,
            ] + \
                game_server.args() + \
                [args for ix, client in enumerate(game_clients) for args in client.args(ix)]
        )

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

class GameClient:

    def __init__(self, **item):
        self.__dict__.update(item)

    def args(self, index):
        return [
            f'host{index}={self.host}',
            f'path{index}={self.path}',
            f'srect{index}={self.srect}',
            f'drect{index}={self.drect}',
            f'args{index}={self.extra_args}',
        ]

class GameServer:

    def __init__(self, **item):
        self.__dict__.update(item)

    def args(self):
        return [
            f'host={self.host}',
            f'path={self.path}',
        ]

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
