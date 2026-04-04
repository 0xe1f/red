#!/usr/bin/env python3

## Copyright (c) 2024 Akop Karapetyan
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##    http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.

from generated.common_pb2 import LaunchId
from generated.responses_pb2 import Result
import argparse
import alsaaudio
import asyncio
import generated.requests_pb2 as requests
import generated.responses_pb2 as responses
import glob
import logging
import nats
import os
import pathlib
import psutil
import re
import shlex
import subprocess
import yaml

SUBJECT = "red.query.*"
LAUNCH_PROCESS_NAME = "pub"
APP_ID_PATTERN = r"^[A-Za-z0-9_-]+$"
TITLE_ID_PATTERN = r"^[A-Za-z0-9_-]+$"
CORE_PATTERN = "*.so"

game_configs = {}
server_config = {}

def read_config(platform_config_path, game_config_path):
    if not pathlib.Path(platform_config_path).is_file():
        raise ValueError(f"Platform configuration file not found: {platform_config_path}")
    if not pathlib.Path(game_config_path).is_file():
        raise ValueError(f"Game configuration file not found: {game_config_path}")

    global server_config
    with open(platform_config_path, 'r') as f:
        server_config = yaml.safe_load(f)

    if 'cores_path' not in server_config:
        raise ValueError("Missing cores path in configuration")

    if 'platforms' not in server_config:
        raise ValueError("Missing platform configuration")

    if 'nats_url' not in server_config:
        raise ValueError("Missing NATS URL in configuration")

    logging.info(f"Server configuration loaded")

    global game_configs
    with open(game_config_path, 'r') as f:
        for game in yaml.safe_load(f):
            if not game.get("app_id"):
                raise ValueError("Game missing app_id")
            if not game.get("title_id"):
                raise ValueError("Game missing title_id")

            id = (game["app_id"], game["title_id"])
            game_configs[id] = game

    logging.info(f"Game configurations loaded for {len(game_configs)} items")

def find_proc_and_tag():
    found_proc = None
    for proc in psutil.process_iter(['name']):
        if proc.info['name'] == LAUNCH_PROCESS_NAME:
            for index, value in enumerate(proc.cmdline()):
                if value == "--tag" or value == "-t":
                    if index + 1 < len(proc.cmdline()):
                        tag = proc.cmdline()[index + 1]
                        if ':' in tag:
                            return proc, tag.split(':', 2)
            logging.warning(f"Process {LAUNCH_PROCESS_NAME} ({proc.pid}) found with no tag")
            found_proc = proc

    return found_proc, None

def query_identifiers():
    _, tag = find_proc_and_tag()
    if not tag:
        return None

    logging.info(f"Process {LAUNCH_PROCESS_NAME} found with tag: {tag}")
    return tag

def current_volume():
    m = alsaaudio.Mixer('PCM')
    vol, _ = m.getvolume()
    logging.info(f"PCM mixer volume obtained: {vol}%")
    return int(vol)

def get_state(data):
    req = requests.StateRequest()
    req.ParseFromString(data)

    _, tag = find_proc_and_tag()
    if tag:
        app_id, title_id = tag
        active = LaunchId(
            app_id=app_id,
            title_id=title_id
        )
    else:
        active = None

    return responses.StateResponse(
        volume=current_volume(),
        active=active,
        is_running=active is not None,
        result=Result(
            status=Result.Status.STATUS_OK,
        )
    )

def stop_process(data):
    req = requests.StopRequest()
    req.ParseFromString(data)

    proc, _ = find_proc_and_tag()
    if not proc:
        logging.warning(f"Process {LAUNCH_PROCESS_NAME} not found")
        return responses.StopResponse(
            was_running=False,
            result=Result(
                status=Result.Status.STATUS_OK,
            )
        )

    logging.info(f"Stopping {LAUNCH_PROCESS_NAME} (pid: {proc.pid})...")

    # Send SIGINT to allow graceful shutdown, then wait for it to exit
    proc.send_signal(psutil.signal.SIGINT)
    try:
        proc.wait(timeout=2)
        logging.info(f"Process interrupted successfully")
    except psutil.TimeoutExpired:
        logging.warning(f"Killing process due to timeout")
        # If it doesn't exit, kill it forcefully
        proc.kill()

    return responses.StopResponse(
        was_running=True,
        result=Result(
            status=Result.Status.STATUS_OK,
        )
    )

def set_volume(data):
    req = requests.SetVolumeRequest()
    req.ParseFromString(data)

    vol = max(0, min(100, int(req.volume)))
    logging.info(f"Setting PCM mixer volume to: {vol}%...")

    m = alsaaudio.Mixer('PCM')
    m.setvolume(vol)

    return responses.SetVolumeResponse(
        volume=current_volume(),
        result=Result(
            status=Result.Status.STATUS_OK,
        )
    )

def launch(data):
    req = requests.LaunchRequest()
    req.ParseFromString(data)

    app_id = req.launch_id.app_id
    title_id = req.launch_id.title_id
    if not re.fullmatch(APP_ID_PATTERN, app_id):
        raise ValueError(f"Invalid app_id: {app_id}")
    if not re.fullmatch(TITLE_ID_PATTERN, title_id):
        raise ValueError(f"Invalid title_id: {title_id}")

    proc, _ = find_proc_and_tag()
    if proc:
        raise ValueError(f"{LAUNCH_PROCESS_NAME} is already running with pid: {proc.pid}")

    logging.info(f"Launching ({app_id}/{title_id})...")

    home_dir = pathlib.Path.home()
    cores_dir = home_dir / server_config['cores_path']

    launch_proc = str(cores_dir / LAUNCH_PROCESS_NAME)
    if not pathlib.Path(launch_proc).is_file():
        raise ValueError(f"Launch executable not found: {launch_proc}")

    core_dir = cores_dir / app_id
    if not core_dir.is_dir():
        raise ValueError(f"Core not found for app_id: {app_id}")

    core_matches = glob.glob(CORE_PATTERN, root_dir=core_dir)
    if not core_matches:
        raise ValueError(f"No core found in {core_dir}")
    so_file = str(core_dir / core_matches[0])

    logging.debug(f"Found core {so_file}")

    # FIXME: configurable rom directory
    rom_dir = core_dir / "roms"
    title_matches = glob.glob(f"{title_id}.*", root_dir=rom_dir)
    if not title_matches:
        raise ValueError(f"No title matching title_id {title_id}")
    rom_path = os.path.relpath(str(rom_dir / title_matches[0]), cores_dir)

    logging.debug(f"Found ROM {rom_path}")

    args = [ launch_proc ]

    platforms_config = server_config.get("platforms", {})
    common_config = platforms_config.get("common", {})
    if common_args := common_config.get("extra_args", ""):
        args.extend(shlex.split(common_args))
    platform_config = platforms_config.get(app_id, {})
    if platform_args := platform_config.get("extra_args", ""):
        args.extend(shlex.split(platform_args))
    if game_args := game_configs.get((app_id, title_id), {}).get("extra_args", ""):
        args.extend(shlex.split(game_args))

    args += [
        "--tag", f"{app_id}:{title_id}",
        "--core", os.path.relpath(so_file, cores_dir),
        "--server-url", server_config['nats_url'],
        rom_path,
    ]

    logging.info(f"Launching {' '.join(args)}")
    with subprocess.Popen(args, start_new_session=True, cwd=cores_dir) as proc:
        logging.info(f"Process launched with pid: {proc.pid}")

    return responses.LaunchResponse(
        result=Result(
            status=Result.Status.STATUS_OK,
        ),
        launch_id=req.launch_id,
    )

def handle_topic(topic, data):
    logging.info(f"Handling request for topic: {topic}")
    response = None
    try:
        match topic:
            case "state":
                response = get_state(data)
            case "set_volume":
                response = set_volume(data)
            case "stop":
                response = stop_process(data)
            case "launch":
                response = launch(data)
            case _:
                raise ValueError(f"Unrecognized topic: {topic}")

    except ValueError as e:
        # FIXME: return a more specific error to client
        logging.error(f"Error handling topic '{topic}': {e}")
        response = responses.GeneralResponse(
            result=Result(
                status=Result.Status.STATUS_ERROR,
            )
        )

    except Exception as e:
        logging.error(f"Unexpected error handling topic '{topic}': {e}")
        response = responses.GeneralResponse(
            result=Result(
                status=Result.Status.STATUS_ERROR,
            )
        )

    return response.SerializeToString()

async def handle_request(msg):
    logging.debug(f"Received request with subject: '{msg.subject}'")

    _, _, topic = msg.subject.rpartition('.')
    response = handle_topic(topic, msg.data)

    await msg.respond(response)

async def start_listening():
    nc = await nats.connect(server_config["nats_url"])

    # Subscribe and get notified of any messages
    await nc.subscribe(SUBJECT, cb=handle_request)
    logging.info(f"Subscribed to '{SUBJECT}'...")

    # Keep the program running to listen for messages
    await asyncio.Future()

def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform-config", "-pc", help="Path to platform configuration file (default: config.yaml)", default="config.yaml")
    parser.add_argument("--game-config", "-gc", help="Path to game configuration file (default: games.yaml)", default="games.yaml")
    parser.add_argument("--output", "-o", help="Path to logging file (default: None, logs to console)")
    parser.add_argument("--output-overwrite", "-oo", help="True to overwrite the logging file (default: False)", action="store_true", default=False)
    parser.add_argument("--log-level", "-l", help="Logging level (default: INFO)", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"])
    args = parser.parse_args()

    # Set up logging
    log_config = {
        'level': getattr(logging, args.log_level),
        'format': '%(asctime)s:%(levelname)s:%(message)s',
        'filename': args.output,
        'filemode': 'w' if args.output_overwrite else 'a'
    }
    logging.basicConfig(**log_config)

    # Load configuration from files
    read_config(args.platform_config, args.game_config)

    asyncio.run(start_listening())

if __name__ == "__main__":
    main()
