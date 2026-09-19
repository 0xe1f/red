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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MODE_NONE = 0,
    MODE_RECORD = 1,
    MODE_PLAYBACK  = 2,
} ReplayMode;

typedef struct Replay Replay;

bool replay_set_frame_type_count(uint8_t count);
bool replay_set_frame_type_shape(uint8_t frame_ix, const void *default_state, uint8_t size);

Replay* replay_start_recording(const char *path);
Replay* replay_continue_recording(const char *path);
Replay* replay_start_playback(const char *path);

void replay_stop(Replay *replay);
void replay_destroy(Replay *replay);
ReplayMode replay_mode(const Replay *replay);

bool replay_read_frame(Replay *replay, uint8_t frame_ix, void *content, size_t size);
bool replay_write_frame(Replay *replay, uint8_t frame_ix, const void *content, size_t size);

void replay_clean_up();

#endif // __REPLAY_H__
