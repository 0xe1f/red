#!/usr/bin/env python3

import config
from flask import Flask, render_template, request
import http
import logging
import subprocess

app = Flask(__name__)

@app.route('/')
def index():
    return render_template(
        'index.html',
        **config.template_args(),
    )

@app.route('/query')
def query():
    result = read_process_output(
        'ssh',
        [
            config.game_server.host,
            f'{config.game_server.path}/query.sh',
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
        config.game_server.host,
        f'{config.game_server.path}/set_volume.sh {volume}',
    ])
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

    game = config.games.get(id)
    if not game:
        logging.error('Game not found')
        return {
            'status': 'ERR',
            'message': 'Game not found',
        }, http.HTTPStatus.NOT_FOUND

    config.game_server.launch(game)
    for client in config.game_clients:
        client.launch(config.game_server.host)

    return { 'status': 'OK'  }

def read_process_output(exe, args):
    result = subprocess.run([exe] + args, stdout=subprocess.PIPE)
    return result.stdout.decode('utf-8').rstrip()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    config = config.Config('config.yaml', 'games.yaml')
    app.run(host='0.0.0.0', port='8080', debug=False)
