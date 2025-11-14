#!/usr/bin/env python3

import config
from collections import defaultdict
from flask import Flask, render_template, request
import http
import logging
import os
import re
import subprocess
import tempfile
from werkzeug.utils import secure_filename

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 128 * 1000 * 1000

ROM_UPLOAD_FILENAME_REGEX = r'^[A-Za-z0-9]+\.[A-Za-z0-9]{1,7}$'
ROM_UPLOAD_ALLOWED_TYPES = [ '.zip', '.7z' ]
ROM_UPLOAD_MAX_FILES = 5

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/filters')
def filters():
    return game_konfig.filters()

@app.route('/games')
def games():
    search = request.args.get("search", "")
    filters = request.args.get("filters", "")
    filter_map = defaultdict(list)
    if filters:
        for (k, v) in [ filter.split(":") for filter in filters.split(',') ]:
            filter_map[k].append(v)

    return game_konfig.games(search=search, filter_map=filter_map)

@app.route('/query')
def query():
    result = read_process_output(
        'ssh',
        [
            konfig.game_server.ip,
            f'{konfig.game_server.path}/query.sh',
        ]
    )
    title, vol, *_ = result.split(' ', 1) + [None] * 2
    return {
        'title': title,
        'volume': vol,
    }

@app.route('/volume', methods=['POST'])
def volume():
    dict = request.json

    volume = dict.get('volume')
    if not 0 <= volume <= 100:
        logging.error(f'{volume} is not a valid volume')
        return {
            'status': 'ERR',
            'message': 'Volume is not valid',
        }, http.HTTPStatus.BAD_REQUEST

    subprocess.call([
        'ssh',
        konfig.game_server.ip,
        f'{konfig.game_server.path}/set_volume.sh {volume}',
    ])
    return { 'status': 'OK' }

@app.route('/stop', methods=['POST'])
def stop():
    konfig.game_server.stop()
    return { 'status': 'OK' }

@app.route('/launch', methods=['POST'])
def launch():
    dict = request.json

    id = dict.get('id')
    if not id:
        logging.error('Missing id')
        return {
            'status': 'ERR',
            'message': 'Game id missing',
        }, http.HTTPStatus.BAD_REQUEST

    game = game_konfig.game_map.get(id)
    if not game:
        logging.error('Game not found')
        return {
            'status': 'ERR',
            'message': 'Game not found',
        }, http.HTTPStatus.NOT_FOUND

    (code, stdout) = konfig.game_server.launch(game)
    if code == 0:
        for client in konfig.game_clients:
            client.launch()
        return {
            'status': 'OK',
        }
    else:
        return {
            'status': 'ERR',
            'message': 'Failed to launch',
            'detail': stdout,
        }, http.HTTPStatus.BAD_REQUEST

@app.route('/upload', methods=['POST'])
def upload():
    # Basic checks
    if len(request.files) < 1:
        return {
            'status': 'ERR',
            'message': 'No files uploaded',
        }, http.HTTPStatus.BAD_REQUEST

    if len(request.files) > ROM_UPLOAD_MAX_FILES:
        return {
            'status': 'ERR',
            'message': 'Too many files',
        }, http.HTTPStatus.BAD_REQUEST

    # Check individual files
    files = []
    for i in range(len(request.files)):
        if f'files[{i}]' not in request.files:
            return {
                'status': 'ERR',
                'message': 'Missing file upload',
            }, http.HTTPStatus.BAD_REQUEST

        file = request.files[f'files[{i}]']
        filename = file.filename
        if not re.fullmatch(ROM_UPLOAD_FILENAME_REGEX, filename):
            return {
                'status': 'ERR',
                'message': f'Bad file name: {filename}',
            }, http.HTTPStatus.BAD_REQUEST

        _, ext = os.path.splitext(filename)
        if ext not in ROM_UPLOAD_ALLOWED_TYPES:
            return {
                'status': 'ERR',
                'message': f'Bad file type: {filename}',
            }, http.HTTPStatus.BAD_REQUEST

        files.append(file)

    with tempfile.TemporaryDirectory() as temp_dir:
        # Save each file in a temp directory
        local_files = []
        for file in files:
            filename = secure_filename(file.filename)
            full_path = os.path.join(temp_dir, filename)

            file.save(full_path)
            local_files.append(full_path)

        # Copy temp files to remote
        args = [
            'scp',
            *local_files,
            f'{konfig.game_server.ip}:{konfig.game_server.rom_path}',
        ]
        subprocess.call(args)

    return { 'status': 'OK' }

def read_process_output(exe, args):
    result = subprocess.run([exe] + args, stdout=subprocess.PIPE)
    return result.stdout.decode('utf-8').rstrip()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    konfig = config.Config('config.yaml')
    game_konfig = config.GameConfig('games.yaml')
    app.run(host='0.0.0.0', port='8080', debug=True)
