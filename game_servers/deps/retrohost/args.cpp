// Copyright (c) 2024 Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libretro.h"
#include "args.h"

static void args_usage(const char *progname);

static void args_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s <game>\n", progname);
}

bool args_parse(int argc, const char **argv, ArgsOptions *opts, KvStore *kv_store)
{
    int i;
    const char **arg;
    opts->output_width = 0;
    opts->output_height = 0;
    opts->max_clients = -1;
    opts->scale_mode = SCALE_MODE_NONE;
    opts->verbose = VERBOSITY_NONE;
    char temp[512];
    for (i = 1, arg = argv + 1; i < argc; i++, arg++) {
        if (strcmp(*arg, "--help") == 0 || strcmp(*arg, "-h") == 0) {
            args_usage(*argv);
            exit(0);
        } else if (strcmp(*arg, "--core") == 0 || strcmp(*arg, "-c") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->so_path = *(++arg);
        } else if (strcmp(*arg, "--output-dims") == 0 || strcmp(*arg, "-wxh") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *value = *(++arg);
            const char *sep = strchr(value, 'x');
            if (sep && value) {
                strncpy(temp, value, sep - value);
                temp[sep - value] = '\0';
                opts->output_width = atoi(temp);
                opts->output_height = atoi(sep + 1);
            }

            if (opts->output_width <= 0 || opts->output_height <= 0) {
                fprintf(stderr, "Invalid output dimensions: '%s'\n", value);
                return false;
            }
        } else if (strcmp(*arg, "--scale-mode") == 0 || strcmp(*arg, "-sm") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *value = *(++arg);
            if (strcmp(value, "shortestxaspect") == 0) {
                opts->scale_mode = SCALE_MODE_SHORTESTXASPECT;
            } else if (strcmp(value, "fit") == 0) {
                opts->scale_mode = SCALE_MODE_FIT;
            } else if (strcmp(value, "half") == 0) {
                opts->scale_mode = SCALE_MODE_HALF;
            } else if (strcmp(value, "none") == 0) {
                opts->scale_mode = SCALE_MODE_NONE;
            } else {
                fprintf(stderr, "Invalid scale mode: '%s'\n", value);
                return false;
            }
        } else if (strncmp(*arg, "--max-clients=", 14) == 0 || strncmp(*arg, "-mc=", 4) == 0) {
            const char *eq = strchr(*arg, '=') + 1;
            opts->max_clients = atoi(eq);
            if (opts->max_clients <= 0) {
                fprintf(stderr, "Invalid max clients: '%s'\n", eq);
                return false;
            }
        } else if (strcmp(*arg, "--background") == 0 || strcmp(*arg, "-background") == 0) {
            opts->background = true;
        } else if (strcmp(*arg, "--show-fps") == 0 || strcmp(*arg, "-fps") == 0) {
            opts->show_fps = true;
        } else if (strcmp(*arg, "--verbose") == 0 || strcmp(*arg, "-v") == 0) {
            opts->verbose = VERBOSITY_NORMAL;
        } else if (strcmp(*arg, "--verbose-extra") == 0 || strcmp(*arg, "-v2") == 0) {
            opts->verbose = VERBOSITY_EXTRA;
        } else if (strcmp(*arg, "--keyvalue") == 0 || strcmp(*arg, "-kv") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *kv = *(++arg);
            const char *eq = strchr(kv, '=');
            strncpy(temp, kv, eq - kv);
            temp[eq - kv] = '\0';

            kvstore_put(kv_store, temp, eq + 1);
        } else if (**arg == '-') {
            fprintf(stderr, "Unrecognized switch: %s\n", *arg);
            return false;
        } else if (opts->rom_path == NULL) {
            opts->rom_path = *arg;
        } else {
            fprintf(stderr, "Unrecognized argument: %s\n", *arg);
            return false;
        }
    }

    return true;
}
