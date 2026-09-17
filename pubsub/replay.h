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
#include <zlib.h>

typedef enum {
    MODE_NONE = 0,
    MODE_RECORD = 1,
    MODE_PLAYBACK  = 2,
} ReplayMode;

// FIXME: this needs to be hidden behind .c
typedef struct {
    ReplayMode mode;
    const char *file_path;
    const char *tmp_path;
    uint64_t input_frame_offset;
    uint64_t input_frame_count;
    double fps;
    int file_fd;
    gzFile gz;
} Replay;

bool replay_set_frame_type_count(uint8_t count);
bool replay_set_frame_type_size(uint8_t frame_ix, uint8_t size);

bool replay_start_recording(Replay *replay, const char *path);
bool replay_continue_recording(Replay *replay, const char *path);
bool replay_start_playback(Replay *replay, const char *path);
void replay_end(Replay *replay);
void replay_abort(Replay *replay);

bool replay_read_input(Replay *replay, void *input_state, size_t size);
bool replay_write_input(Replay *replay, const void *input_state, size_t size);

void replay_clean_up();

#endif // __REPLAY_H__
