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

import os
if os.environ.get('PRODUCTION', False):
    import eventlet
    eventlet.monkey_patch()

# A handle on real OS threads, for work that must not run as a greenlet (see
# VideoSession). Don't key this off PRODUCTION: gunicorn's eventlet worker
# patches threading itself, before this module is imported.
import threading as os_threading
try:
    import eventlet.patcher
    if eventlet.patcher.is_monkey_patched('thread'):
        os_threading = eventlet.patcher.original('threading')
except ImportError:
    pass

from collections import defaultdict
from flask import session
from generated.common_pb2 import LaunchId
from generated.responses_pb2 import Result
import argparse
import asyncio
import config
import flask
import flask_login
import flask_socketio
import generated.responses_pb2 as responses
import generated.requests_pb2 as requests
import generated.replay_pb2 as replay
import http
import importlib
import logging
import nats
import re
import struct
import subprocess
import threading
import time

app = flask.Flask(__name__)

socketio = flask_socketio.SocketIO(app)

import data.launches as dl

import users

RESPONSE_PACKAGE_NAME = "generated.responses_pb2"
ROM_UPLOAD_FILENAME_REGEX = r'^[A-Za-z0-9]+\.[A-Za-z0-9]{1,7}$'
ROM_UPLOAD_ALLOWED_TYPES = [ '.zip', '.7z' ]
ROM_UPLOAD_MAX_FILES = 5

platform_config_file = None
games_config_file = None
games_config_last_modified = 0

def deserialize(message, type):
    module_, class_ = type.rsplit('.', 1)
    class_ = getattr(importlib.import_module(module_), class_)
    rv = class_()
    rv.ParseFromString(message)
    return rv

async def request_server_async(request):
    subject = f"red.requests"
    logging.info(f"Requesting '{subject}'")

    nc = await nats.connect(konfig.game_server['nats_url'])
    try:
        encoded_data = request.SerializeToString()
        response = await nc.request(subject, encoded_data, timeout=0.5)
        resp = responses.ResponseEnvelope()
        resp.ParseFromString(response.data)

        logging.debug(f"Received response for subject '{subject}': {resp}")
    except TimeoutError:
        # FIXME: return a more specific error to client
        logging.error("Request timed out")

    await nc.close()

    return resp

def request_server(request):
    return asyncio.run(request_server_async(request))

async def request_topic_async(topic, request, response_type):
    subject = f"red.query.{topic}"
    logging.info(f"Requesting subject '{subject}'")

    nc = await nats.connect(konfig.game_server['nats_url'])
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

@app.route('/record', methods=['POST'])
@flask_login.login_required
def record():
    slot = flask.request.json.get('slot')
    slot = max(0, min(slot, 9)) if isinstance(slot, int) else 0

    try:
        response = request_server(
            requests.RequestEnvelope(
                replay_record=replay.ReplayRecordRequest(
                    slot = slot
                )
            )
        )

    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return {
            'status': 'ERR',
            'message': 'Unexpected error',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

    return {
        'status': 'OK' if response is not None and response.error_code == 0 else 'ERR',
    }

@app.route('/playback', methods=['POST'])
@flask_login.login_required
def playback():
    slot = flask.request.json.get('slot')
    slot = max(0, min(slot, 9)) if isinstance(slot, int) else 0

    try:
        response = request_server(
            requests.RequestEnvelope(
                replay_playback=replay.ReplayPlaybackRequest(
                    slot = slot
                )
            )
        )

    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return {
            'status': 'ERR',
            'message': 'Unexpected error',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

    return {
        'status': 'OK' if response is not None and response.error_code == 0 else 'ERR',
    }

@app.route('/stop_replay', methods=['POST'])
@flask_login.login_required
def stop_replay():
    try:
        response = request_server(
            requests.RequestEnvelope(
                replay_stop=replay.ReplayStopRequest()
            )
        )

    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return {
            'status': 'ERR',
            'message': 'Unexpected error',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

    return {
        'status': 'OK' if response is not None and response.error_code == 0 else 'ERR',
    }

@app.route('/resume_record', methods=['POST'])
@flask_login.login_required
def resume_record():
    slot = flask.request.json.get('slot')
    slot = max(0, min(slot, 9)) if isinstance(slot, int) else 0

    try:
        response = request_server(
            requests.RequestEnvelope(
                replay_resume_record=replay.ReplayResumeRecordRequest(
                    slot = slot
                )
            )
        )

    except Exception as e:
        logging.error(f"Unexpected error: {e}")
        return {
            'status': 'ERR',
            'message': 'Unexpected error',
        }, http.HTTPStatus.INTERNAL_SERVER_ERROR

    return {
        'status': 'OK' if response is not None and response.error_code == 0 else 'ERR',
    }

VIDEO_SUBJECT = "red.video.frames"
VIDEO_KEEPALIVE_SECONDS = 1.0
VIDEO_POLL_SECONDS = 0.01

class VideoSession:
    """One NATS subscription to the video feed, shared by all viewers.

    Frames are written by the pump thread; viewers poll for the newest one, so
    a slow viewer skips frames instead of building up a backlog.
    """

    def __init__(self):
        self.stop = os_threading.Event()
        self.latest = None
        self.seq = 0
        self.error = None
        self.viewers = 0

    def start(self, nats_url):
        # The pump runs asyncio in a real OS thread. A greenlet won't do: under
        # eventlet, greenlets share one OS thread, so a long-lived event loop in
        # one would make asyncio.run() fail everywhere else with "cannot be
        # called from a running event loop".
        os_threading.Thread(target=self._run, args=(nats_url,), daemon=True).start()

    def _run(self, nats_url):
        try:
            asyncio.run(self._pump(nats_url))
        except Exception as e:
            logging.error(f"Video relay failed: {e}")
            self.error = e

    async def _pump(self, nats_url):
        nc = await nats.connect(nats_url)

        async def on_frame(msg):
            self.latest = bytes(msg.data)
            self.seq += 1

        try:
            sub = await nc.subscribe(VIDEO_SUBJECT, cb=on_frame)
            while not self.stop.is_set():
                await asyncio.sleep(0.1)
            await sub.unsubscribe()
        finally:
            await nc.close()

class VideoRelay:

    def __init__(self):
        self.lock = threading.Lock()
        self.session = None

    def join(self):
        with self.lock:
            if self.session is None:
                self.session = VideoSession()
                self.session.start(konfig.game_server['nats_url'])
            self.session.viewers += 1
            return self.session

    def leave(self, session):
        with self.lock:
            session.viewers -= 1
            if session.viewers == 0:
                session.stop.set()
                if self.session is session:
                    self.session = None

video_relay = VideoRelay()

@app.route('/video/stream')
@flask_login.login_required
def video_stream():
    """Relays published video frames to the browser in real time.

    The body is a never-ending sequence of messages, each a little-endian u32
    length followed by that many bytes of frame (header + LZ4 pixel data, exactly
    as published on NATS). A zero length is a keepalive with no payload.

    The launcher is subscribed to NATS only while at least one viewer is
    connected.
    """
    def generate():
        feed = video_relay.join()
        last_seq = 0
        last_sent = time.monotonic()
        try:
            while feed.error is None:
                seq, data = feed.seq, feed.latest
                if seq != last_seq and data is not None:
                    last_seq = seq
                    last_sent = time.monotonic()
                    yield struct.pack('<I', len(data)) + data
                elif time.monotonic() - last_sent >= VIDEO_KEEPALIVE_SECONDS:
                    # Also how a disconnected viewer is noticed when idle
                    last_sent = time.monotonic()
                    yield struct.pack('<I', 0)
                else:
                    time.sleep(VIDEO_POLL_SECONDS)
        finally:
            video_relay.leave(feed)

    return flask.Response(generate(), mimetype='application/octet-stream', headers={
        'Cache-Control': 'no-store',
        'X-Accel-Buffering': 'no',
    })

@app.before_request
def check_configs_for_changes():
    modified = os.path.getmtime(games_config_file)
    if modified != games_config_last_modified:
        logging.info("Game config file changed, reloading...")
        reload_game_config()

def read_process_output(exe, args):
    result = subprocess.run([exe] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        stderr = result.stderr.decode('utf-8').rstrip()
        raise RuntimeError(f'Process {exe} failed: {stderr}')
    return result.stdout.decode('utf-8').rstrip()

def reload_game_config():
    global game_konfig, games_config_last_modified
    games_config_last_modified = os.path.getmtime(games_config_file)
    game_konfig = config.GameConfig(games_config_file)
    logging.info(f"Loaded game config with {len(game_konfig.game_map)} items")

def init_service():
    global konfig, platform_config_file, games_config_file

    # Parse command-line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform-config", "-pc", help="Path to platform configuration file (default: config.yaml)", default="config.yaml")
    parser.add_argument("--game-config", "-gc", help="Path to game configuration file (default: games.yaml)", default="games.yaml")
    parser.add_argument("--output", "-o", help="Path to logging file (default: None, logs to console)")
    parser.add_argument("--output-overwrite", "-oo", help="True to overwrite the logging file (default: False)", action="store_true", default=False)
    parser.add_argument("--log-level", "-l", help="Logging level (default: INFO)", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"])
    args, _ = parser.parse_known_args()

    # Set up logging
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format='%(asctime)s:%(levelname)s:%(message)s',
        filename=args.output,
        filemode='w' if args.output_overwrite else 'a',
    )

    platform_config_file = args.platform_config
    games_config_file = args.game_config

    konfig = config.Config(platform_config_file)
    reload_game_config()

    app.config.update(konfig.control_server['flask_config'])

init_service()

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port='8080', debug=True)
