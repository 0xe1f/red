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
import asyncio
import config
import flask
import flask_login
import generated.common_pb2 as pbcom
import generated.requests_pb2 as pbreq
import generated.responses_pb2 as pbresp
import http
import importlib
import logging
import nats
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
    result = request_topic("state", pbreq.StateRequest(), "generated.responses_pb2.StateResponse")

    # FIXME: error handling
    active = result.active
    tag = f"{active.app_id}:{active.title_id}" if result.is_running else None

    return {
        'title': game_konfig.game_map.get(tag).as_dict() if tag else None,
        'is_running': result.is_running,
        'volume': result.volume,
    }

@app.route('/sync')
@flask_login.login_required
def sync():
    orientation = "UNKNOWN"
    if konfig.sensor.device:
        response = read_process_output("./sensor.py", [ f"--device={konfig.sensor.device}", "orientation" ])
        orientation = response

    return {
        'orientation': orientation,
    }

@app.route('/volume', methods=['POST'])
@flask_login.login_required
def volume():
    dict = flask.request.json

    volume = dict.get('volume')
    volume = max(0, min(volume, 100)) if isinstance(volume, int) else None
    if volume is None:
        logging.error('Missing or invalid volume')
        return {
            'status': 'ERR',
            'message': 'Missing or invalid volume',
        }, http.HTTPStatus.BAD_REQUEST

    result = request_topic("set_volume", pbreq.SetVolumeRequest(volume=volume), "generated.responses_pb2.SetVolumeResponse")

    return { 'volume': result.volume }

def deserialize(message, type):
    module_, class_ = type.rsplit('.', 1)
    class_ = getattr(importlib.import_module(module_), class_)
    rv = class_()
    rv.ParseFromString(message)
    return rv

async def request_topic_async(topic, request, response_type):
    subject = f"red.query.{topic}"
    logging.info(f"Requesting subject '{subject}'")

    # FIXME: nats URL
    nc = await nats.connect(f"nats://{konfig.game_server.ip}:4222")
    try:
        encoded_data = request.SerializeToString()
        response = await nc.request(subject, encoded_data, timeout=0.5)
        response_obj = deserialize(response.data, response_type)
        logging.debug(f"Received response for subject '{subject}': {response_obj}")
    except TimeoutError:
        # FIXME: return a more specific error to client
        logging.error("Request timed out")

    # Terminate connection to NATS.
    await nc.drain()

    return response_obj

def request_topic(topic, request, response_type):
    return asyncio.run(request_topic_async(topic, request, response_type))

@app.route('/stop', methods=['POST'])
@flask_login.login_required
def stop():
    request_topic("stop", pbreq.StopRequest(), "generated.responses_pb2.StopResponse")

    # FIXME: return a proper status
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

    state = request_topic("state", pbreq.StateRequest(), "generated.responses_pb2.StateResponse")
    if state.is_running:
        request_topic("stop", pbreq.StopRequest(), "generated.responses_pb2.GeneralResponse")
        dl.end_launch()

    title = game_konfig.game_map.get(id)
    if not title:
        logging.error('Game not found')
        return {
            'status': 'ERR',
            'message': 'Game not found',
        }, http.HTTPStatus.NOT_FOUND

    response = request_topic("launch", pbreq.LaunchRequest(
        launch_id=pbcom.LaunchId(
            app_id=title.app_id,
            title_id=title.title_id,
        ),
    ), "generated.responses_pb2.GeneralResponse")

    if response.result.status == pbresp.Result.Status.STATUS_OK:
        dl.create_launch(session_id=session.get('id'), uid=title.id)
        dl.increment_launch_count(uid=title.id)
        return {
            'status': 'OK',
            'title': title.as_dict(),
        }
    else:
        return {
            'status': 'ERR',
            'message': 'Failed to launch',
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

def read_process_output(exe, args):
    result = subprocess.run([exe] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        stderr = result.stderr.decode('utf-8').rstrip()
        raise RuntimeError(f'Process {exe} failed: {stderr}')
    return result.stdout.decode('utf-8').rstrip()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    konfig = config.Config('config.yaml')
    game_konfig = config.GameConfig('games.yaml')
    app.run(host='0.0.0.0', port='8080', debug=True)
