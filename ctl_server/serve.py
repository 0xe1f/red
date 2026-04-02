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
from generated.common_pb2 import LaunchId
from generated.responses_pb2 import Result
import argparse
import asyncio
import config
import flask
import flask_login
import generated.requests_pb2 as requests
import http
import importlib
import logging
import nats
import re
import subprocess

app = flask.Flask(__name__)

import data.launches as dl

import users

RESPONSE_PACKAGE_NAME = "generated.responses_pb2"
ROM_UPLOAD_FILENAME_REGEX = r'^[A-Za-z0-9]+\.[A-Za-z0-9]{1,7}$'
ROM_UPLOAD_ALLOWED_TYPES = [ '.zip', '.7z' ]
ROM_UPLOAD_MAX_FILES = 5

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
    nc = await nats.connect(f"nats://{konfig.game_server['ip']}:4222")
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

def request_topic(request, topic=None, response_type=None):
    request_class_name = request.__class__.__name__.removesuffix('Request')
    if topic is None:
        topic = re.sub(r'(?<!^)(?=[A-Z])', '_', request_class_name).lower()
    if response_type is None:
        response_type = f"{RESPONSE_PACKAGE_NAME}.{request_class_name}Response"

    logging.debug(f"Requesting '{topic}' with response type of '{response_type}'")
    return asyncio.run(request_topic_async(topic, request, response_type))

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
    try:
        result = request_topic(requests.StateRequest())
    except Exception as e:
        logging.error(f"Failed to query game state: {e}")
        return {
            'status': 'ERR',
            'message': 'Failed to query game state',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

    active = result.active
    tag = f"{active.app_id}:{active.title_id}" if result.is_running else None

    return {
        'title': game_konfig.game_map.get(tag).as_dict() if tag else None,
        'is_running': result.is_running,
        'volume': result.volume,
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

    try:
        result = request_topic(requests.SetVolumeRequest(volume=volume))
    except Exception as e:
        logging.error(f"Failed to set volume: {e}")
        return {
            'status': 'ERR',
            'message': 'Failed to set volume',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

    return { 'volume': result.volume }

@app.route('/stop', methods=['POST'])
@flask_login.login_required
def stop():
    try:
        request_topic(requests.StopRequest())
    except Exception as e:
        logging.error(f"Failed to stop game: {e}")
        return {
            'status': 'ERR',
            'message': 'Failed to stop game',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

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

    if not (title := game_konfig.game_map.get(id)):
        logging.error('Game not found')
        return {
            'status': 'ERR',
            'message': 'Game not found',
        }, http.HTTPStatus.NOT_FOUND

    try:
        state = request_topic(requests.StateRequest())
        if state.is_running:
            request_topic(requests.StopRequest())
            dl.end_launch()

        response = request_topic(
            requests.LaunchRequest(
                launch_id=LaunchId(
                    app_id=title.app_id,
                    title_id=title.title_id,
                ),
            )
        )

    except Exception as e:
        logging.error(f"Error during launch: {e}")
        return {
            'status': 'ERR',
            'message': 'Failed to launch game',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

    if response.result.status == Result.Status.STATUS_OK:
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
    # # Basic checks
    # if len(flask.request.files) < 1:
    #     return {
    #         'status': 'ERR',
    #         'message': 'No files uploaded',
    #     }, http.HTTPStatus.BAD_REQUEST

    # if len(flask.request.files) > ROM_UPLOAD_MAX_FILES:
    #     return {
    #         'status': 'ERR',
    #         'message': 'Too many files',
    #     }, http.HTTPStatus.BAD_REQUEST

    # # Check individual files
    # files = []
    # for i in range(len(flask.request.files)):
    #     if f'files[{i}]' not in flask.request.files:
    #         return {
    #             'status': 'ERR',
    #             'message': 'Missing file upload',
    #         }, http.HTTPStatus.BAD_REQUEST

    #     file = flask.request.files[f'files[{i}]']
    #     filename = file.filename
    #     if not re.fullmatch(ROM_UPLOAD_FILENAME_REGEX, filename):
    #         return {
    #             'status': 'ERR',
    #             'message': f'Bad file name: {filename}',
    #         }, http.HTTPStatus.BAD_REQUEST

    #     _, ext = os.path.splitext(filename)
    #     if ext not in ROM_UPLOAD_ALLOWED_TYPES:
    #         return {
    #             'status': 'ERR',
    #             'message': f'Bad file type: {filename}',
    #         }, http.HTTPStatus.BAD_REQUEST

    #     files.append(file)

    # with tempfile.TemporaryDirectory() as temp_dir:
    #     # Save each file in a temp directory
    #     local_files = []
    #     for file in files:
    #         filename = secure_filename(file.filename)
    #         full_path = os.path.join(temp_dir, filename)

    #         file.save(full_path)
    #         local_files.append(full_path)

    #     # Copy temp files to remote
    #     args = [
    #         'scp',
    #         *local_files,
    #         f'{konfig.game_server.ip}:{konfig.game_server.rom_path}',
    #     ]
    #     subprocess.call(args)

    # return { 'status': 'OK' }
    raise NotImplementedError("ROM upload is not implemented yet")

@app.route('/sync')
@flask_login.login_required
def sync():
    orientation = "UNKNOWN"
    if device := konfig.sensor.get('device'):
        response = read_process_output("./sensor.py", [ f"--device={device}", "orientation" ])
        orientation = response

    return {
        'orientation': orientation,
    }

def read_process_output(exe, args):
    result = subprocess.run([exe] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        stderr = result.stderr.decode('utf-8').rstrip()
        raise RuntimeError(f'Process {exe} failed: {stderr}')
    return result.stdout.decode('utf-8').rstrip()

def main():
    global konfig, game_konfig

    # Parse command-line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform-config", "-pc", help="Path to platform configuration file (default: config.yaml)", default="config.yaml")
    parser.add_argument("--game-config", "-gc", help="Path to game configuration file (default: games.yaml)", default="games.yaml")
    parser.add_argument("--output", "-o", help="Path to logging file (default: None, logs to console)")
    parser.add_argument("--output-overwrite", "-oo", help="True to overwrite the logging file (default: False)", action="store_true", default=False)
    parser.add_argument("--log-level", "-l", help="Logging level (default: INFO)", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"])
    args = parser.parse_args()

    # Set up logging
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format='%(asctime)s:%(levelname)s:%(message)s',
        filename=args.output,
        filemode='w' if args.output_overwrite else 'a',
    )

    konfig = config.Config(args.platform_config)
    game_konfig = config.GameConfig(args.game_config)
    app.config.update(konfig.control_server['flask_config'])

    app.run(host='0.0.0.0', port=8080, debug=True)

if __name__ == '__main__':
    main()
