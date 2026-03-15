# retrohost

**retrohost** is a lightweight server designed to host and run libretro cores
for [red](https://github.com/0xe1f/red/). It's a frontend tailored specifically
to the red device and its operation.

It loads libretro `.so`'s, manages their lifecycle, handles configuration
application (largely via command-line).

## Building

    ```sh
    cd game_servers/deps/retrohost
    make
    ```

## Running

Example command line:

```sh
./rh -c bluemsx/bluemsx_libretro.so \
    bluemsx/roms/monty.rom \
    -v \
    -fps \
    --output-dims=320x256 \
    -kv "bluemsx_msxtype=MSX2 - Yamaha YIS805-128R2"
```

## Command-Line Arguments

| Argument | Alias | Value | Description |
| --- | --- | --- | --- |
| `<game>` | - | ROM/game path | Positional ROM/game path (`opts->rom_path`). |
| `--help` | `-h` | None | Show usage and exit. |
| `--core` | `-c` | `<path>` | Path to libretro core `.so`. |
| `--output-dims` | `-wxh` | `<width>x<height>` | Display dimensions. Both values must be positive. |
| `--scale-mode` | `-sm` | `shortestxaspect \| fit \| half \| none` | Output scaling mode. |
| `--max-clients=<n>` | `-mc=<n>` | Positive integer | Maximum number of clients. |
| `--background` | `-background` | None | Run in background mode. |
| `--show-fps` | `-fps` | None | Enable once-per-second FPS logging. |
| `--verbose` | `-v` | None | Enable normal verbosity logging. |
| `--verbose-extra` | `-v2` | None | Enable extra verbosity logging. |
| `--keyvalue` | `-kv` | `<key=value>` | Set a core option key/value pair (can be reused, usage core-dependent). |
