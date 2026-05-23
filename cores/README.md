libretro cores
===

This directory contains libretro cores, plus build helpers used to stage,
build, and deploy core binaries to the game server.

## Prerequisites

- A valid `deploy.yaml` in the repository root (see `deploy.yaml.example`).
- `yq` installed on the machine running `bcores.sh`.
- SSH access:
  - local machine -> build server
  - build server -> game server
- `rsync` available on local/build/game hosts.
- Build dependencies for each core installed on the build server.

## Build and deploy

From repository root:

```bash
./bcores.sh core_a core_b core_c ...
```

If no core names are passed, `bcores.sh` attempts to build all top-level core
directories under `cores`.

## How module build detection works

For each selected core, `cores/build_module.sh` uses:

1. Custom build script found in `_build`, named `[core_name].build`
2. `Makefile.libretro` anywhere in the module tree
3. First `Makefile` found under a `libretro` subdirectory
4. First `Makefile` found anywhere in the module

Note that some projects may require special build steps to complete build successfully - for those, use a tailored build script (see 1).

When using a custom build script, ensure to output name of the `.so` module, as well as any other files that need to be deployed, as the last line of output:

```text
done:/path/to/core_libretro.so
```


## Special directories

- `cores/_build`: Custom build script.
- `cores/_exclude`: `rsync` files to exclude when deploying to build server. Useful to avoid unnecessary copying of alternative platform sources/unused binaries.

Exclusion behavior:

- `cores/_exclude/common.exclude` applies to all modules.
- `cores/_exclude/core_name.exclude` applies only to a specific core.

## Adding a new core

Most of the time, checking out a new core under `cores` and running `bcores.sh name` is sufficient.

Generally speaking:

1. Add the core source under `cores` as a top-level directory.
2. Ensure it is buildable by one of:
   - `Makefile.libretro`
   - a `libretro` `Makefile` path
   - a general `Makefile`
   - custom `cores/_build` script
3. If needed, create per-core exclusions in `cores/_exclude`.
4. Run:

```bash
./bcores.sh new_core_name
```

## Custom build scripts

Use a custom build script when auto-detection is not enough.

- Place script at `cores/_build` with filename `core_name.build`.
- Script should perform all required build steps and finish by echoing:

```text
done:path_to_output_libretro_so
```

See [existing scripts](_build) for an example.

## License

```
   Copyright 2024 Akop Karapetyan

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```
