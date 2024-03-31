#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, HTTPServer
from jinja2 import Environment, FileSystemLoader
import json
import logging
import mimetypes
import re

class S(BaseHTTPRequestHandler):

    def write_response_headers(self, code = 200, type = 'text/html'):
        self.send_response(code)
        self.send_header('Content-type', type)
        self.end_headers()

    def do_GET(self):
        logging.info("GET request,\nPath: %s\nHeaders:\n%s\n", str(self.path), str(self.headers))

        m = simple_path_capture.match(self.path)
        if m and m.group(1) == 'static':
            self.serve_static_file("{}/{}".format(m.group(1), m.group(2)))
            return

        self.write_response_headers()
        content = template.render(
            games = games,
        )
        self.wfile.write(content.encode('utf-8'))

    def do_POST(self):
        m = simple_path_capture.match(self.path)
        if m and m.group(1) == 'action':
            self.handle_action(m.group(2))

        self.write_response_headers()

    def serve_static_file(self, path):
        # TODO: Cache type
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


def run(server_class=HTTPServer, handler_class=S, port=8080):
    logging.basicConfig(level=logging.INFO)
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    logging.info('Starting httpd...\n')
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    logging.info('Stopping httpd...\n')

environment = Environment(loader=FileSystemLoader("templates/"))
template = environment.get_template("main.jinja")
simple_path_capture = re.compile('/([a-zA-Z0-9_.]{1,64})/([a-zA-Z0-9_.]{1,64})')

class Game:
    def __init__(self, id, title):
        self.id = id
        self.title = title

games = [
    Game('pacman', 'Pacman'),
    Game('digdug', 'Dig Dug'),
]

if __name__ == '__main__':
    from sys import argv

    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()
