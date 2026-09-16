rsub
===
A long-running LED-matrix subscriber for frames published by
[pubsub](../pubsub)'s `pub`. This is the Rust version for C `ledd`.

It connects to NATS, subscribes to `red.frames`, LZ4-decompresses each
payload, and blits a source rectangle onto this panel. Frames are always
published at a fixed size; each subscriber chooses how much of the frame
to display. All scaling, filtering and compression is done pre-publish.

## Requirements

* Rust (stable), including `cargo`
* A NATS server reachable at `--server-url`
* On the Pi: GPIO access (typically `sudo`) and an RGB LED matrix
  compatible with [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix)

```
cargo build --release
```

Build on a Raspberry Pi (aarch64). Other hosts get C stubs for the matrix
library and will not drive panels.

## Usage

Example usage:

```
sudo ./target/release/rsub \
  --server-url nats://192.168.1.101:4222 \
  --led-rows=64 \
  --led-cols=64 \
  --led-chain=5 \
  --led-parallel=2 \
  --led-slowdown-gpio=4 \
  --led-gpio-mapping=regular \
  --led-pwm-bits=6 \
  --src-rect 0,0-320,128 \
  --dest-rect 0,0-320,128 \
  --content-rect 0,0-320,256
```

`--src-rect` / `--dest-rect` / `--content-rect` are `x1,y1-x2,y2`. LED-matrix
flags (`--led-*`) match hzeller's library; see `rsub --help`.

Logging: `-l` / `--log-level` (fatal, error, warn, info, debug, verbose)
applies to this crate. Other crates default to warn. `RUST_LOG` overrides
the filter when set (for example `RUST_LOG=rsub=trace,async_nats=debug`).

## License

   Copyright 2026, Akop Karapetyan

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
