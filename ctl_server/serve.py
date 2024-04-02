#!/usr/bin/env python3
import glob
import http.server
import http
import jinja2
import json
import logging
import mimetypes
import os
import urllib.parse
import yaml

template_path = 'templates'
templates = {}
games = []

class Handler(http.server.BaseHTTPRequestHandler):

    def write_response_headers(self, code = 200, type = 'text/html'):
        self.send_response(code)
        self.send_header('Content-type', type)
        self.end_headers()

    def do_GET(self):
        logging.info("GET request,\nPath: %s\nHeaders:\n%s\n", str(self.path), str(self.headers))

        path = os.path.normpath(self.path).lstrip('/')
        if path.startswith('static/'):
            self.serve_static_file(path)
        else:
            template = templates.get(path if path else 'index')
            if not template:
                self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")
                return

            content = template.render(
                games = games
            )
            self.write_response_headers()
            self.wfile.write(content.encode('utf-8'))

    def do_POST(self):
        path = urllib.parse.urlparse(self.path).path.lstrip('/')
        if path.startswith('action'):
            self.handle_action(path.rsplit('/', 1)[-1])
        else:
            self.send_error(http.HTTPStatus.NOT_FOUND, "Not found")
            return

        self.write_response_headers()

    def serve_static_file(self, path):
        logging.info("serve_static_file: %s", path)
        mt = mimetypes.guess_type(path)
        self.write_response_headers(type = mt[0])
        with open(path, 'rb') as file:
            self.wfile.write(file.read())

    def handle_action(self, action):
        logging.info("handle_action: %s", action)

        if action == 'launch':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length).decode('utf-8')
            payload = json.loads(post_data)

            self.launch(payload['id'])

    def launch(self, id):
        logging.info('launch: %s', id)

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
            games.append(Game(**item))

init_templates()
init_games()

if __name__ == '__main__':
    from sys import argv

    if len(argv) == 2:
        run(port = int(argv[1]))
    else:
        run()
