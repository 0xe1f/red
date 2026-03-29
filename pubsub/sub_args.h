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

#ifndef __SUB_ARGS_H__
#define __SUB_ARGS_H__

#include <stdbool.h>
#include "log.h"
#include "matrix.h"

typedef struct {
    const char *server_url;
    const char *log_path;
    bool log_overwrite;
    bool background;
    bool show_fps;
    LogLevel log_level;
    ViewRect content;
    ViewRect source;
    ViewRect dest;
} ArgsOptions;

bool args_parse(int argc, const char **argv, ArgsOptions *opts);
void args_free(ArgsOptions *opts);

#endif // __SUB_ARGS_H__
