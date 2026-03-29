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

#include <stdlib.h>
#include <string.h>

#include "sub_args.h"

#define LOG_TAG "args"

bool args_parse(int argc, const char **argv, ArgsOptions *opts)
{
    opts->server_url = NULL;
    opts->log_path = NULL;
    opts->log_level = LOG_INFO;
    opts->log_overwrite = false;
    opts->background = false;
    opts->show_fps = false;

    int i;
    const char **arg;
    for (i = 1, arg = argv + 1; i < argc; i++, arg++) {
        if (strcmp(*arg, "--help") == 0) {
            fprintf(stdout, "Usage: %s <server-url> [--src-rect=x1,y1-x2,y2] [--dest-rect=x1,y1-x2,y2] [--content-rect=x1,y1-x2,y2] [--background] [--fps]\n", argv[0]);
            fprintf(stdout, "  --src-rect          Source rectangle in server bitmap\n");
            fprintf(stdout, "  --dest-rect         Destination rectangle on LED matrix\n");
            fprintf(stdout, "  --content-rect      Content rectangle on LED matrix\n");
            fprintf(stdout, "  --background        Run in background as a daemon\n");
            fprintf(stdout, "  --fps               Show server FPS\n");
            return false;
        } else if (strcmp(*arg, "--show-fps") == 0 || strcmp(*arg, "-fps") == 0) {
            opts->show_fps = true;
        } else if (strcmp(*arg, "--background") == 0 || strcmp(*arg, "-bg") == 0) {
            opts->background = true;
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
        } else if (strcmp(*arg, "--server-url") == 0 || strcmp(*arg, "-s") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->server_url = *(++arg);
        } else if (strcmp(*arg, "--output") == 0 || strcmp(*arg, "-o") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->log_path = *(++arg);
        } else if (strcmp(*arg, "--output-overwrite") == 0 || strcmp(*arg, "-oo") == 0) {
            opts->log_overwrite = true;
        } else if (strcmp(*arg, "--src-rect") == 0 || strcmp(*arg, "-sr") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *post_eq = *(++arg);
            if (!viewrect_parse(post_eq, &opts->source)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            } else if (!viewrect_validate(&opts->source)) {
                log_f(LOG_TAG, "Invalid source rectangle\n");
                return false;
            } 
        } else if (strcmp(*arg, "--dest-rect") == 0 || strcmp(*arg, "-dr") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *post_eq = *(++arg);
            if (!viewrect_parse(post_eq, &opts->dest)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            } else if (!viewrect_validate(&opts->dest)) {
                log_f(LOG_TAG, "Invalid destination rectangle\n");
                return false;
            }
        } else if (strcmp(*arg, "--content-rect") == 0 || strcmp(*arg, "-cr") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *post_eq = *(++arg);
            if (!viewrect_parse(post_eq, &opts->content)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            } else if (!viewrect_validate(&opts->content)) {
                log_f(LOG_TAG, "Invalid content rectangle\n");
                return false;
            }
        } else if (strcmp(*arg, "--output") == 0 || strcmp(*arg, "-o") == 0) {
            if (++i >= argc) {
                log_e(LOG_TAG, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->log_path = *(++arg);
        } else if (strcmp(*arg, "--output-overwrite") == 0 || strcmp(*arg, "-oo") == 0) {
            opts->log_overwrite = true;
        } else if (strstr(*arg, "--led-")) {
            continue; // skip led matrix options
        } else if (strstr(*arg, "-") == *arg) {
            log_f(LOG_TAG, "Unrecognized switch: '%s'\n", *arg);
            return false;
        } else {
            log_e(LOG_TAG, "Unrecognized argument: %s\n", *arg);
            return false;
        }
    }

    if (!opts->server_url) {
         log_f(LOG_TAG, "Missing server URL\n");
         return false;
    } else if (viewrect_is_zero(&opts->source)) {
        log_f(LOG_TAG, "Missing source rectangle\n");
        return false;
    } else if (viewrect_is_zero(&opts->dest)) {
        log_f(LOG_TAG, "Missing destination rectangle\n");
        return false;
    }  else if (viewrect_is_zero(&opts->content)) {
        log_f(LOG_TAG, "Missing content rectangle\n");
        return false;
    }

    return true;
}

void args_free(ArgsOptions *opts)
{
    // ...
}
