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

import serial
from argparse import ArgumentParser

device = ''
rate = 9600
timeout = 1

def read_args():
    global device, rate, timeout

    parser = ArgumentParser(description='Query Arduino sensor')
    parser.add_argument('request', help='Request to send to device')
    parser.add_argument('--device', required=True, help='Serial device path')
    parser.add_argument('--rate', type=int, default=rate, help='Baud rate')
    parser.add_argument('--timeout', type=int, default=timeout, help='Timeout in seconds')

    args = parser.parse_args()

    device = args.device
    rate = args.rate
    timeout = args.timeout

    return args.request

def query(request: str) -> str:
    ser = serial.Serial(device, rate, timeout=timeout)
    ser.reset_input_buffer()

    ser.write((request + '\n').encode('utf-8'))
    response = ser.readline().decode('utf-8').rstrip()
    parts = response.split(':', maxsplit=1)
    if len(parts) != 2 or parts[0] != request:
        raise RuntimeError(f'Invalid response: {response}')
    return parts[1]

def main():
    request = read_args()
    response = query(request)
    print(response)

if __name__ == '__main__':
    main()
