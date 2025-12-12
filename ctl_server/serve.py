#!/usr/bin/env python3

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
from flask import session
from werkzeug.utils import secure_filename
import config
import flask
import flask_login
import http
import logging
import os
import re
import subprocess
import tempfile
import tomllib

app = flask.Flask(__name__)
app.config.from_file("config.toml", load=tomllib.load, text=False)

import data.launches as dl

import users

ROM_UPLOAD_FILENAME_REGEX = r'^[A-Za-z0-9]+\.[A-Za-z0-9]{1,7}$'
ROM_UPLOAD_ALLOWED_TYPES = [ '.zip', '.7z' ]
ROM_UPLOAD_MAX_FILES = 5

@app.route('/')
@flask_login.login_required
def index():
    return flask.render_template('index.html')

@app.route('/filters')
@flask_login.login_required
def filters():
    return game_konfig.filters()

@app.route('/games')
@flask_login.login_required
def games():
    search = flask.request.args.get("search", "")
    filters = flask.request.args.get("filters", "")
    filter_map = defaultdict(list)
    if filters:
        for (k, v) in [ filter.split(":") for filter in filters.split(',') ]:
            filter_map[k].append(v)

    return game_konfig.games(search=search, filter_map=filter_map)

@app.route('/query')
@flask_login.login_required
def query():
    return server_state()

@app.route('/volume', methods=['POST'])
@flask_login.login_required
def volume():
    dict = flask.request.json

    volume = dict.get('volume')
    if not 0 <= volume <= 100:
        logging.error(f'{volume} is not a valid volume')
        return {
            'status': 'ERR',
            'message': 'Volume is not valid',
        }, http.HTTPStatus.BAD_REQUEST

    result = read_process_output(
        'ssh',
        [
            konfig.game_server.ip,
            f'{konfig.game_server.path}/set_volume.sh {volume}',
        ]
    )
    vol = int(result) if result.isdigit() else None
    return { 'volume': vol }

@app.route('/stop', methods=['POST'])
@flask_login.login_required
def stop():
    if server_state()['is_running']:
        konfig.game_server.stop()
        dl.end_launch()

    return { 'status': 'OK' }

@app.route('/launch', methods=['POST'])
@flask_login.login_required
def launch():
    dict = flask.request.json

    if not (id := dict.get('id')):
        logging.error('Missing id')
        return {
            'status': 'ERR',
            'message': 'Game id missing',
        }, http.HTTPStatus.BAD_REQUEST

    if server_state()['is_running']:
        dl.end_launch()

    title = game_konfig.game_map.get(id)
    if not title:
        logging.error('Game not found')
        return {
            'status': 'ERR',
            'message': 'Game not found',
        }, http.HTTPStatus.NOT_FOUND

    (code, stdout) = konfig.game_server.launch(title)
    if code == 0:
        dl.create_launch(
            session_id=session.get('id'),
            app_id=title.app_id,
            title_id=title.id,
        )
        dl.increment_launch_count(
            app_id=title.app_id,
            title_id=title.id,
        )
        for client in konfig.game_clients:
            client.launch()
        return {
            'status': 'OK',
            'title': title.as_dict(),
        }
    else:
        return {
            'status': 'ERR',
            'message': 'Failed to launch',
            'detail': stdout,
        }, http.HTTPStatus.BAD_REQUEST

@app.route('/upload', methods=['POST'])
@flask_login.login_required
def upload():
    # Basic checks
    if len(flask.request.files) < 1:
        return {
            'status': 'ERR',
            'message': 'No files uploaded',
        }, http.HTTPStatus.BAD_REQUEST

    if len(flask.request.files) > ROM_UPLOAD_MAX_FILES:
        return {
            'status': 'ERR',
            'message': 'Too many files',
        }, http.HTTPStatus.BAD_REQUEST

    # Check individual files
    files = []
    for i in range(len(flask.request.files)):
        if f'files[{i}]' not in flask.request.files:
            return {
                'status': 'ERR',
                'message': 'Missing file upload',
            }, http.HTTPStatus.BAD_REQUEST

        file = flask.request.files[f'files[{i}]']
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

def server_state():
    result = read_process_output(
        'ssh',
        [
            konfig.game_server.ip,
            f'{konfig.game_server.path}/query.sh',
        ]
    )
    _, title_id, vol, *_ = result.split(' ') + [None] * 4
    title = game_konfig.game_map.get(title_id) if title_id else None
    return {
        'title': title.as_dict() if title else None,
        'is_running': not not title_id,
        'volume': vol,
    }

def read_process_output(exe, args):
    result = subprocess.run([exe] + args, stdout=subprocess.PIPE)
    return result.stdout.decode('utf-8').rstrip()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    konfig = config.Config('config.yaml')
    game_konfig = config.GameConfig('games.yaml')
    app.run(host='0.0.0.0', port='8080', debug=True)
