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

import argparse
import alsaaudio
import asyncio
import json
import logging
import nats
import psutil

PUB_PROCESS_NAME = "pub"

def find_proc_and_tag():
    found_proc = None
    for proc in psutil.process_iter(['name']):
        if proc.info['name'] == PUB_PROCESS_NAME:
            for index, value in enumerate(proc.cmdline()):
                if value == "--tag" or value == "-t":
                    if index + 1 < len(proc.cmdline()):
                        tag = proc.cmdline()[index + 1]
                        if ':' in tag:
                            return proc, tag.split(':', 2)
            logging.warning(f"Process {PUB_PROCESS_NAME} ({proc.pid}) found with no tag")
            found_proc = proc

    return found_proc, None

def query_identifiers():
    _, tag = find_proc_and_tag()
    if not tag:
        return None

    logging.info(f"Process {PUB_PROCESS_NAME} found with tag: {tag}")
    return tag

def current_volume():
    m = alsaaudio.Mixer('PCM')
    vol, _ = m.getvolume()
    logging.info(f"PCM mixer volume obtained: {vol}%")
    return int(vol)

def get_state():
    _, tag = find_proc_and_tag()
    volume = current_volume()

    if tag:
        app_id, title = tag
        active = {
            "app_id": app_id,
            "title": title
        }
    else:
        active = None

    out = {
        "volume": volume,
        "result": "OK"
    }
    if active:
        out["active"] = active

    return out

def stop_process():
    proc, _ = find_proc_and_tag()
    if not proc:
        logging.warning(f"Process {PUB_PROCESS_NAME} not found")
        return False

    logging.info(f"Stopping {PUB_PROCESS_NAME} (pid: {proc.pid})...")

    # Send SIGINT to allow graceful shutdown, then wait for it to exit
    proc.send_signal(psutil.signal.SIGINT)
    try:
        proc.wait(timeout=2)
        logging.info(f"Process interrupted successfully")
    except psutil.TimeoutExpired:
        logging.warning(f"Killing process due to timeout")
        # If it doesn't exit, kill it forcefully
        proc.kill()

    return {
        "result": "OK"
    }

def set_volume(vol):
    vol = max(0, min(100, vol))
    m = alsaaudio.Mixer('PCM')
    m.setvolume(vol)
    logging.info(f"PCM mixer volume set to: {vol}%")
    return {
        "volume": current_volume(),
        "result": "OK"
    }

def unknown_query(subject):
    logging.warning(f"Query unrecognized: {subject}")
    return {
        "result": "ERROR",
        "message": "Unknown query"
    }

def handle_request(subject, data):
    logging.info(f"Handling request for subject: {subject}")
    _, _, result = subject.rpartition('.')
    match result:
        case "state":
            return json.dumps(get_state())
        case "set_volume":
            # FIXME: properly validate input
            if not data.isdigit():
                # FIXME: return a proper error response
                return "Invalid volume value. Must be an integer between 0 and 100."
            else:
                vol = int(data)
                return json.dumps(set_volume(vol))
        case "stop":
            return json.dumps(stop_process())
        case _:
            return json.dumps(unknown_query(subject))

async def reply_handler(msg):
    data = msg.data.decode()
    logging.debug(f"Received request: '{data}' with subject: '{msg.subject}'")
    response = handle_request(msg.subject, data)
    await msg.respond(response.encode())

async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", "-s", help="NATS server URL (default: nats://localhost:4222)", default="nats://localhost:4222")

    args = parser.parse_args()

    nc = await nats.connect(args.server)
    # Subscribe and get notified of any messages on "red.query.*"
    await nc.subscribe("red.query.*", cb=reply_handler)
    logging.info("Subscribed to 'red.query.*'...")
    # Keep the program running to listen for messages
    await asyncio.Future()

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='%(asctime)s:%(levelname)s:%(message)s')
    asyncio.run(main())
