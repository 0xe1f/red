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

#ifndef __PUB_ARGS_H__
#define __PUB_ARGS_H__

#include "kv_store.h"
#include "video.h"
#include "input.h"
#include "log.h"

typedef struct {
    const char *server_url;
    const char *rom_path;
    const char *so_path;
    const char *log_path;
    const char *mouse_device_path;
    const char *keyboard_device_path;
    bool log_overwrite;
    bool background;
    bool show_fps;
    int output_width;
    int output_height;
    LogLevel log_level;
    ScaleMode scale_mode;
    bool disable_preloading;
    bool force_preloading;
    KvStore input_configs;
    DeferredKeypress *autopress;
    const char *tag;
} ArgsOptions;

bool args_parse(int argc, const char **argv, ArgsOptions *opts, KvStore *kv_store);
void args_free(ArgsOptions *opts);

#endif // __PUB_ARGS_H__
