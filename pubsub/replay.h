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

#ifndef __REPLAY_H__
#define __REPLAY_H__

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    MODE_NONE = 0,
    MODE_RECORD = 1,
    MODE_PLAYBACK  = 2,
} ReplayMode;

typedef struct {
    ReplayMode mode;
    FILE *file;
} Replay;

bool replay_start_recording(Replay *replay, const char *path);
bool replay_start_playback(Replay *replay, const char *path);
void replay_stop(Replay *replay);

bool replay_read_input(Replay *replay, void *input_state, size_t size);
bool replay_write_input(Replay *replay, const void *input_state, size_t size);

#endif // __REPLAY_H__
