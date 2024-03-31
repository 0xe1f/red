#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, HTTPServer
from jinja2 import Environment, FileSystemLoader
import logging
import mimetypes
import re
import urllib

class S(BaseHTTPRequestHandler):

    def _set_response(self, type = 'text/html'):
        self.send_response(200)
        self.send_header('Content-type', type)
        self.end_headers()

    def do_GET(self):
        logging.info("GET request,\nPath: %s\nHeaders:\n%s\n", str(self.path), str(self.headers))
        m = simple_path_capture.match(self.path)
        if m and m.group(1) == 'static':
            self.serve_static_file("{}/{}".format(m.group(1), m.group(2)))
            return

        self._set_response()
        content = template.render(
            games = games,
        )
        self.wfile.write(content.encode('utf-8'))

    def do_POST(self):
        content_length = int(self.headers['Content-Length']) # <--- Gets the size of data
        post_data = self.rfile.read(content_length) # <--- Gets the data itself
        logging.info("POST request,\nPath: %s\nHeaders:\n%s\n\nBody:\n%s\n",
                str(self.path), str(self.headers), post_data.decode('utf-8'))

        self._set_response()
        self.wfile.write("POST request for {}".format(self.path).encode('utf-8'))

    def serve_static_file(self, path):
        # TODO: Cache type
        logging.info("serve_static_file: %s", path)
        mt = mimetypes.guess_type(path)
        self._set_response(mt)
        with open(path, 'rb') as file:
            self.wfile.write(file.read())

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
template = environment.get_template("fafnir.jinja")
simple_path_capture = re.compile('/([a-zA-Z0-9_.]{1,64})/([a-zA-Z0-9_.]{1,64})')

class Game:
    def __init__(self, title):
        self.title = title

games = [
    Game('Pacman'),
    Game('Dig Dug'),
]

if __name__ == '__main__':
    from sys import argv

    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()
