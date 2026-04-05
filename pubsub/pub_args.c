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
#include "log.h"
#include "pub_args.h"

#define LOG_TAG "args"

static void args_usage(const char *progname);

static void args_usage(const char *progname)
{
    printf("Usage: %s <game>\n", progname);
}

bool args_parse(int argc, const char **argv, ArgsOptions *opts, KvStore *kv_store)
{
    int i;
    const char **arg;
    opts->output_width = 0;
    opts->output_height = 0;
    opts->scale_mode = SCALE_MODE_NONE;
    opts->log_level = LOG_INFO;
    opts->log_overwrite = false;
    opts->background = false;
    opts->show_fps = false;
    char temp[1024];
    static DeferredKeypress dk;
    for (i = 1, arg = argv + 1; i < argc; i++, arg++) {
        if (strcmp(*arg, "--help") == 0 || strcmp(*arg, "-h") == 0) {
            args_usage(*argv);
            exit(0);
        } else if (strcmp(*arg, "--core") == 0 || strcmp(*arg, "-c") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->so_path = *(++arg);
        } else if (strcmp(*arg, "--output-dims") == 0 || strcmp(*arg, "-wxh") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
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
                log_e(LOG_TAG, "Invalid output dimensions: '%s'\n", value);
                return false;
            }
        } else if (strcmp(*arg, "--scale-mode") == 0 || strcmp(*arg, "-sm") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
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
                log_e(LOG_TAG, "Invalid scale mode: '%s'\n", value);
                return false;
            }
        } else if (strcmp(*arg, "--background") == 0 || strcmp(*arg, "-bg") == 0) {
            opts->background = true;
        } else if (strcmp(*arg, "--show-fps") == 0 || strcmp(*arg, "-fps") == 0) {
            opts->show_fps = true;
        } else if (strcmp(*arg, "--log-level") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *level = *(++arg);
            if (strcmp(level, "fatal") == 0) {
                opts->log_level = LOG_FATAL;
            } else if (strcmp(level, "error") == 0) {
                opts->log_level = LOG_ERROR;
            } else if (strcmp(level, "warn") == 0) {
                opts->log_level = LOG_WARN;
            } else if (strcmp(level, "info") == 0) {
                opts->log_level = LOG_INFO;
            } else if (strcmp(level, "debug") == 0) {
                opts->log_level = LOG_DEBUG;
            } else if (strcmp(level, "verbose") == 0) {
                opts->log_level = LOG_VERBOSE;
            } else {
                log_e(LOG_TAG, "Invalid log level: '%s'\n", level);
                return false;
            }
        } else if (strncmp(*arg, "-l", 2) == 0) {
            const char *level = *arg + 2;
            if (strcmp(level, "f") == 0) {
                opts->log_level = LOG_FATAL;
            } else if (strcmp(level, "e") == 0) {
                opts->log_level = LOG_ERROR;
            } else if (strcmp(level, "w") == 0) {
                opts->log_level = LOG_WARN;
            } else if (strcmp(level, "i") == 0) {
                opts->log_level = LOG_INFO;
            } else if (strcmp(level, "d") == 0) {
                opts->log_level = LOG_DEBUG;
            } else if (strcmp(level, "v") == 0) {
                opts->log_level = LOG_VERBOSE;
            } else if (strcmp(level, "") == 0) {
                log_e(LOG_TAG, "Log level unspecified\n");
                return false;
            } else {
                log_e(LOG_TAG, "Invalid log level: '%s'\n", level);
                return false;
            }
        } else if (strcmp(*arg, "--keyvalue") == 0 || strcmp(*arg, "-kv") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *kv = *(++arg);
            const char *eq = strchr(kv, '=');
            strncpy(temp, kv, eq - kv);
            temp[eq - kv] = '\0';

            kvstore_put(kv_store, temp, eq + 1);
        } else if (strcmp(*arg, "--disable-preloading") == 0 || strcmp(*arg, "-np") == 0) {
            opts->disable_preloading = true;
        } else if (strcmp(*arg, "--force-preloading") == 0 || strcmp(*arg, "-fp") == 0) {
            opts->force_preloading = true;
        } else if (strcmp(*arg, "--input-config") == 0 || strcmp(*arg, "-ic") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }

            const char *value = *(++arg);
            const char *sep = strchr(value, ':');
            if (!sep) {
                log_e(LOG_TAG, "Invalid input config: '%s'\n", value);
                return false;
            }

            strncpy(temp, value, sep - value);
            temp[sep - value] = '\0';
            kvstore_put(&opts->input_configs, temp, sep + 1);
        } else if (strcmp(*arg, "--output") == 0 || strcmp(*arg, "-o") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->log_path = *(++arg);
        } else if (strcmp(*arg, "--output-overwrite") == 0 || strcmp(*arg, "-oo") == 0) {
            opts->log_overwrite = true;
        } else if (strcmp(*arg, "--autopress") == 0 || strcmp(*arg, "-ap") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            if (!input_parse_deferred_keypress(*(++arg), &dk)) {
                log_e(LOG_TAG, "Invalid autopress spec: '%s'\n", *arg);
                return false;
            }
            opts->autopress = &dk;
        } else if (strcmp(*arg, "--mouse-device") == 0 || strcmp(*arg, "-mouse") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->mouse_device_path = *(++arg);
        } else if (strcmp(*arg, "--keyboard-device") == 0 || strcmp(*arg, "-kbd") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->keyboard_device_path = *(++arg);
        } else if (strcmp(*arg, "--server-url") == 0 || strcmp(*arg, "-s") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->server_url = *(++arg);
        } else if (strcmp(*arg, "--tag") == 0 || strcmp(*arg, "-t") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            // A tag is for external applications to identify critical data.
            // We'll record it, but not use it for anything internally.
            opts->tag = *(++arg);
        } else if (strcmp(*arg, "--chatty-core") == 0 || strcmp(*arg, "-cc") == 0) {
            opts->chatty_core = true;
        } else if (**arg == '-') {
            log_e(LOG_TAG, "Unrecognized switch: %s\n", *arg);
            return false;
        } else if (opts->rom_path == NULL) {
            opts->rom_path = *arg;
        } else {
            log_e(LOG_TAG, "Unrecognized argument: %s\n", *arg);
            return false;
        }
    }

    return true;
}

void args_free(ArgsOptions *opts)
{
    kvstore_free(&opts->input_configs);
}
